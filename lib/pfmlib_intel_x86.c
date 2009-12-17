/* pfmlib_intel_x86.c : common code for Intel X86 processors
 *
 * Copyright (c) 2009 Google, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file implements the common code for all Intel X86 processors.
 */
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/* private headers */
#include "pfmlib_priv.h"
#include "pfmlib_intel_x86_priv.h"

const pfmlib_attr_desc_t intel_x86_mods[]={
	PFM_ATTR_B("u", "monitor at priv level 1, 2, 3"),	/* monitor priv level 1, 2, 3 */
	PFM_ATTR_B("k", "monitor at priv level 0"),		/* monitor priv level 0 */
	PFM_ATTR_B("i", "invert"),				/* invert */
	PFM_ATTR_B("e", "edge level"),				/* edge */
	PFM_ATTR_I("c", "counter-mask in range [0-255]"),	/* counter-mask */
	PFM_ATTR_B("t", "measure any thread"),			/* montor on both threads */
	PFM_ATTR_NULL /* end-marker to avoid exporting number of entries */
};
#define mod_name(a) intel_x86_mods[(a)].name

static int intel_x86_arch_version;

int
intel_x86_detect(int *family, int *model)
{
	int ret;
	char buffer[128];

	ret = pfmlib_getcpuinfo_attr("vendor_id", buffer, sizeof(buffer));
	if (ret == -1)
		return PFM_ERR_NOTSUPP;

	/* must be Intel */
	if (strcmp(buffer, "GenuineIntel"))
		return PFM_ERR_NOTSUPP;

	ret = pfmlib_getcpuinfo_attr("cpu family", buffer, sizeof(buffer));
	if (ret == -1)
		return PFM_ERR_NOTSUPP;

	if (family)
		*family = atoi(buffer);

	ret = pfmlib_getcpuinfo_attr("model", buffer, sizeof(buffer));
	if (ret == -1)
		return PFM_ERR_NOTSUPP;

	if (model)
		*model = atoi(buffer);
	return PFM_SUCCESS;
}

int
intel_x86_add_defaults(const intel_x86_entry_t *ent, char *umask_str, unsigned int msk, unsigned int *umask)
{
	int i, j, added;

	for(i=0; msk; msk >>=1, i++) {

		if (!(msk & 0x1))
			continue;

		added = 0;

		for(j=0; j < ent->numasks; j++) {

			if (ent->umasks[j].grpid != i)
				continue;

			if (ent->umasks[j].uflags & INTEL_X86_DFL) {
				DPRINT("added default %s for group %d\n", ent->umasks[j].uname, i);

				*umask |= ent->umasks[j].ucode;

				evt_strcat(umask_str, ":%s", ent->umasks[j].uname);

				added++;
			}
		}
		if (!added) {
			DPRINT("no default found for event %s unit mask group %d\n", ent->name, i);
			return PFM_ERR_UMASK;
		}
	}
	return PFM_SUCCESS;
}

int
intel_x86_encode_gen(void *this, pfmlib_event_desc_t *e, pfm_intel_x86_reg_t *reg)
{
	pfmlib_attr_t *a;
	const intel_x86_entry_t *pe;
	unsigned int grpmsk, ugrpmsk = 0;
	uint64_t val;
	unsigned int umask;
	unsigned int modhw = 0;
	unsigned int plmmsk = 0;
	int k, ret, grpid, last_grpid = -1;
	int grpcounts[INTEL_X86_NUM_GRP];
	int ncombo[INTEL_X86_NUM_GRP];
	char umask_str[PFMLIB_EVT_MAX_NAME_LEN];

	memset(grpcounts, 0, sizeof(grpcounts));
	memset(ncombo, 0, sizeof(ncombo));

	pe = this_pe(this);

	umask_str[0] = e->fstr[0] = '\0';

	/*
	 * preset certain fields from event code
	 */
	val   = pe[e->event].code;

	grpmsk = (1 << pe[e->event].ngrp)-1;
	reg->val |= val;

	/* take into account hardcoded umask */
	umask = (val >> 8) & 0xff;

	for(k=0; k < e->nattrs; k++) {
		a = e->attrs+k;
		if (a->type == PFM_ATTR_UMASK) {
			grpid = pe[e->event].umasks[a->id].grpid;

			/*
			 * certain event groups are meant to be
			 * exclusive, i.e., only unit masks of one group
			 * can be used
			 */
			if (last_grpid != -1 && grpid != last_grpid
			    && intel_x86_eflag(this, e, INTEL_X86_GRP_EXCL)) {
				DPRINT("exclusive unit mask group error\n");
				return PFM_ERR_FEATCOMB;
			}

			/*
			 * upper layer has removed duplicates
			 * so if we come here more than once, it is for two
			 * disinct umasks
			 *
			 * NCOMBO=no combination of unit masks within the same
			 * umask group
			 */
			++grpcounts[grpid];

			/* mark that we have a umask with NCOMBO in this group */
			if (intel_x86_uflag(this, e, a->id, INTEL_X86_NCOMBO))
				ncombo[grpid] = 1;

			/*
			 * if more than one umask in this group but one is marked
			 * with ncombo, then fail. It is okay to combine umask within
			 * a group as long as none is tagged with NCOMBO
			 */
			if (grpcounts[grpid] > 1 && ncombo[grpid])  {
				DPRINT("event does not support unit mask combination within a group\n");
				return PFM_ERR_FEATCOMB;
			}

			/*
			 * if umask is derivative of another one, use the original
			 */
			if (pe[e->event].umasks[a->id].ufrom)
				evt_strcat(umask_str, ":%s", pe[e->event].umasks[a->id].ufrom);
			else
				evt_strcat(umask_str, ":%s", pe[e->event].umasks[a->id].uname);

			last_grpid = grpid;
			modhw    |= pe[e->event].umasks[a->id].modhw;
			umask    |= pe[e->event].umasks[a->id].ucode;
			ugrpmsk  |= 1 << pe[e->event].umasks[a->id].grpid;

			reg->val |= umask << 8;

			
		} else {
			switch(a->id) {
				case INTEL_X86_ATTR_I: /* invert */
					reg->sel_inv = !!a->ival;
					break;
				case INTEL_X86_ATTR_E: /* edge */
					reg->sel_edge = !!a->ival;
					break;
				case INTEL_X86_ATTR_C: /* counter-mask */
					if (a->ival > 255)
						return PFM_ERR_ATTR_VAL;
					reg->sel_cnt_mask = a->ival;
					break;
				case INTEL_X86_ATTR_U: /* USR */
					reg->sel_usr = !!a->ival;
					plmmsk |= _INTEL_X86_ATTR_U;
					break;
				case INTEL_X86_ATTR_K: /* OS */
					reg->sel_os = !!a->ival;
					plmmsk |= _INTEL_X86_ATTR_K;
					break;
				case INTEL_X86_ATTR_T: /* anythread (v3 and above) */
					if (reg->sel_anythr)
						return PFM_ERR_ATTR_SET;
					reg->sel_anythr = !!a->ival;
					break;
			}
		}
	}

	if ((modhw & _INTEL_X86_ATTR_I) && reg->sel_inv)
		return PFM_ERR_ATTR_HW;
	if ((modhw & _INTEL_X86_ATTR_E) && reg->sel_edge)
		return PFM_ERR_ATTR_HW;
	if ((modhw & _INTEL_X86_ATTR_C) && reg->sel_cnt_mask)
		return PFM_ERR_ATTR_HW;
	if ((modhw & _INTEL_X86_ATTR_T) && reg->sel_anythr)
		return PFM_ERR_ATTR_HW;
	if ((modhw & _INTEL_X86_ATTR_U) && reg->sel_usr)
		return PFM_ERR_ATTR_HW;
	if ((modhw & _INTEL_X86_ATTR_K) && reg->sel_os)
		return PFM_ERR_ATTR_HW;

	/*
	 * handle case where no priv level mask was passed.
	 * then we use the dfl_plm
	 */
	if (!(plmmsk & (_INTEL_X86_ATTR_K|_INTEL_X86_ATTR_U))) {
		if (e->dfl_plm & PFM_PLM0)
			reg->sel_os = 1;
		if (e->dfl_plm & PFM_PLM3)
			reg->sel_usr = 1;
	}

	/*
	 * check that there is at least of unit mask in each unit
	 * mask group
	 */
	if (ugrpmsk != grpmsk) {
		ugrpmsk ^= grpmsk;
		ret = intel_x86_add_defaults(pe+e->event, umask_str, ugrpmsk, &umask);
		if (ret != PFM_SUCCESS)
			return ret;
	}
	reg->sel_en        = 1; /* force enable bit to 1 */
	reg->sel_int       = 1; /* force APIC int to 1 */

	if (pe[e->event].from)
		evt_strcat(e->fstr, "%s", pe[e->event].from);
	else
		evt_strcat(e->fstr, "%s", pe[e->event].name);

	/*
	 * decode unit masks
	 */
	umask = reg->sel_unit_mask;
	for(k=0; k < pe[e->event].numasks; k++) {
		unsigned int um, msk;
		/*
		 * skip alias unit mask, it means there is an equivalent
		 * unit mask.
		 */
		if (pe[e->event].umasks[k].ufrom)
			continue;

		/* just the unit mask  and none of the hardwired modifiers */
		um = pe[e->event].umasks[k].ucode & 0xff;

		/*
		 * extract grp bitfield mask used to exclude groups
		 * of bits
		 */
		msk = pe[e->event].umasks[k].grpmsk;
		if (!msk)
			msk = 0xff;
		/*
		 * if umasks is NCOMBO, then it means the umask code must match
		 * exactly (is the only one allowed) and therefore it consumes
		 * the full bit width of the group.
		 *
		 * Otherwise, we match individual bits, ane we only remove the
		 * matching bits, because there can be combinations.
		 */
		if (intel_x86_uflag(this, e, k, INTEL_X86_NCOMBO)) {
			if ((umask & msk) == um) {
				evt_strcat(e->fstr, ":%s", pe[e->event].umasks[k].uname);
				umask &= ~msk;
			}
		} else {
			if (umask & msk) {
				evt_strcat(e->fstr, ":%s", pe[e->event].umasks[k].uname);
				umask &= ~um;
			}
		}
	}
	/*
	 * decode modifiers
	 */
	evt_strcat(e->fstr, ":%s=%"PRIu64, mod_name(INTEL_X86_ATTR_K), reg->sel_os);
	evt_strcat(e->fstr, ":%s=%"PRIu64, mod_name(INTEL_X86_ATTR_U), reg->sel_usr);
	evt_strcat(e->fstr, ":%s=%"PRIu64, mod_name(INTEL_X86_ATTR_E), reg->sel_edge);
	evt_strcat(e->fstr, ":%s=%"PRIu64, mod_name(INTEL_X86_ATTR_I), reg->sel_inv);
	evt_strcat(e->fstr, ":%s=%"PRIu64, mod_name(INTEL_X86_ATTR_C), reg->sel_cnt_mask);

	if (intel_x86_arch_version > 2)
		evt_strcat(e->fstr, ":%s=%"PRIu64, mod_name(INTEL_X86_ATTR_T), reg->sel_anythr);


	return PFM_SUCCESS;
}

int
pfm_intel_x86_get_event_code(void *this, int i, uint64_t *code)
{
	const intel_x86_entry_t *pe = this_pe(this);
	*code = pe[i].code;
	return PFM_SUCCESS;
}

const char *
pfm_intel_x86_get_event_name(void *this, int i)
{
	const intel_x86_entry_t *pe = this_pe(this);
	return pe[i].name;
}

const char *
pfm_intel_x86_get_event_desc(void *this, int ev)
{
	const intel_x86_entry_t *pe = this_pe(this);
	return pe[ev].desc;
}

const char *
pfm_intel_x86_get_event_umask_name(void *this, int e, int attr)
{
	const intel_x86_entry_t *pe = this_pe(this);
	return pe[e].umasks[attr].uname;
}

const char *
pfm_intel_x86_get_event_umask_desc(void *this, int e, int attr)
{
	const intel_x86_entry_t *pe = this_pe(this);
	return pe[e].umasks[attr].udesc;
}

int
pfm_intel_x86_get_event_umask_code(void *this, int e, int attr, uint64_t *code)
{
	const intel_x86_entry_t *pe = this_pe(this);
	*code = pe[e].umasks[attr].ucode;
	return PFM_SUCCESS;
}

int
pfm_intel_x86_get_encoding(void *this, pfmlib_event_desc_t *e, uint64_t *codes, int *count, pfmlib_perf_attr_t *attrs)
{
	const intel_x86_entry_t *pe = this_pe(this);
	pfm_intel_x86_reg_t reg;
	int ret;

	/*
	 * If event requires special encoding, then invoke
	 * model specific encoding function
	 */
	if (intel_x86_eflag(this, e, INTEL_X86_ENCODER))
		return pe[e->event].encoder(this, e, codes, count, attrs);


	reg.val = 0; /* not initialized by encode function */

	ret = intel_x86_encode_gen(this, e, &reg);
	if (ret != PFM_SUCCESS)
		return ret;

	*codes = reg.val;
	*count = 1;

	if (attrs) {
		if (reg.sel_os)
			attrs->plm |= PFM_PLM0;
		if (reg.sel_usr)
			attrs->plm |= PFM_PLM3;
	}

	__pfm_vbprintf("[0x%"PRIx64" event_sel=0x%x umask=0x%x os=%d usr=%d en=%d int=%d inv=%d edge=%d cnt_mask=%d] %s\n",
			reg.val,
			reg.sel_event_select,
			reg.sel_unit_mask,
			reg.sel_os,
			reg.sel_usr,
			reg.sel_en,
			reg.sel_int,
			reg.sel_inv,
			reg.sel_edge,
			reg.sel_cnt_mask,
			e->fstr);

	return PFM_SUCCESS;
}

int
pfm_intel_x86_get_event_numasks(void *this, int idx)
{
	const intel_x86_entry_t *pe = this_pe(this);
	return pe[idx].numasks;
}

int
pfm_intel_x86_get_event_first(void *this)
{
	return 0;
}

int
pfm_intel_x86_get_event_next(void *this, int idx)
{
	pfmlib_pmu_t *p = this;

	if (idx >= (p->pme_count-1))
		return -1;

	return idx+1;
}

int
pfm_intel_x86_event_is_valid(void *this, int pidx)
{
	pfmlib_pmu_t *p = this;
	return pidx >= 0 && pidx < p->pme_count;
}

/*
 * The following functions are specific to this PMU and are exposed directly
 * to the user
 */
int
pfm_event_supports_pebs(const char *str)
{
	int i, ret, umask_pebs = 1;
	pfmlib_event_desc_t e;

	if (PFMLIB_INITIALIZED() == 0)
		return PFM_ERR_NOTSUPP;

	ret = pfmlib_parse_event(str, &e);
	if (ret != PFM_SUCCESS)
		return ret;

	/*
	 * check event-level first
	 * if no unit masks, then we are done
	 */
	if (intel_x86_eflag(e.pmu, &e, INTEL_X86_PEBS) && e.nattrs == 0)
		return PFM_SUCCESS;

	/*
	 * ALL unit masks must support PEBS
	 */
	for(i=0; i < e.nattrs; i++)
		if (e.attrs[i].type == PFM_ATTR_UMASK)
			umask_pebs &= intel_x86_uflag(e.pmu, &e, e.attrs[i].id, INTEL_X86_PEBS);
	
	return umask_pebs ? PFM_SUCCESS : PFM_ERR_NOTSUPP;
}

void
intel_x86_display_reg(void *this, pfm_intel_x86_reg_t reg, int c, int event)
{
	const intel_x86_entry_t *pe = this_pe(this);

	/*
	 * handle generic counters
	 */
	__pfm_vbprintf("[PERFEVTSEL%u=0x%"PRIx64" event_sel=0x%x umask=0x%x os=%d usr=%d "
		       "en=%d int=%d inv=%d edge=%d cnt_mask=%d",
			c,
			reg.val,
			reg.sel_event_select,
			reg.sel_unit_mask,
			reg.sel_os,
			reg.sel_usr,
			reg.sel_en,
			reg.sel_int,
			reg.sel_inv,
			reg.sel_edge,
			reg.sel_cnt_mask);

	switch(intel_x86_arch_version) {
	case 3:
		/* v3 adds anythread */
		__pfm_vbprintf(" any=%d", reg.sel_anythr);
		break;
	default:
		break;
	}
	__pfm_vbprintf("] %s\n", pe[event].name);
}

int
pfm_intel_x86_get_event_perf_type(void *this, int pidx)
{
	return PERF_TYPE_RAW;
}

pfmlib_modmsk_t
pfm_intel_x86_get_event_modifiers(void *this, int pidx)
{
	const intel_x86_entry_t *pe = this_pe(this);

	return pe[pidx].modmsk;
}

int
pfm_intel_x86_validate_table(void *this, FILE *fp)
{
	pfmlib_pmu_t *pmu = this;
	const intel_x86_entry_t *pe = this_pe(this);
	int i, j, k, error = 0;

	for(i=0; i < pmu->pme_count; i++) {

		if (!pe[i].name) {
			fprintf(fp, "pmu: %s event%d: :: no name\n", pmu->name, i);
			error++;
		}

		if (!pe[i].desc) {
			fprintf(fp, "pmu: %s event%d: %s :: no description\n", pmu->name, i, pe[i].name);
			error++;
		}

		if (!pe[i].cntmsk) {
			fprintf(fp, "pmu: %s event%d: %s :: cntmsk=0\n", pmu->name, i, pe[i].name);
			error++;
		}

		if (pe[i].numasks >= INTEL_X86_NUM_UMASKS) {
			fprintf(fp, "pmu: %s event%d: %s :: numasks too big (<%d)\n", pmu->name, i, pe[i].name, INTEL_X86_NUM_UMASKS);
			error++;
		}

		if (pe[i].numasks && pe[i].ngrp == 0) {
			fprintf(fp, "pmu: %s event%d: %s :: ngrp cannot be zero\n", pmu->name, i, pe[i].name);
			error++;
		}

		if (pe[i].ngrp >= INTEL_X86_NUM_GRP) {
			fprintf(fp, "pmu: %s event%d: %s :: ngrp too big (max=%d)\n", pmu->name, i, pe[i].name, INTEL_X86_NUM_GRP);
			error++;
		}

		for(j=0; j < pe[i].numasks; j++) {

			if (!pe[i].umasks[j].uname) {
				fprintf(fp, "pmu: %s event%d: umask%d :: no name\n", pmu->name, i, j);
				error++;
			}
			if (pe[i].umasks[j].modhw && (pe[i].umasks[j].modhw | pe[i].modmsk) != pe[i].modmsk) {
				fprintf(fp, "pmu: %s event%d: %s umask%d: %s :: modhw not subset of modmsk\n", pmu->name, i, pe[i].name, j, pe[i].umasks[j].uname);
				error++;
			}

			if (!pe[i].umasks[j].udesc) {
				fprintf(fp, "pmu: %s event%d: umask%d: %s :: no description\n", pmu->name, i, j, pe[i].umasks[j].uname);
				error++;
			}

			if (pe[i].ngrp && pe[i].umasks[j].grpid >= pe[i].ngrp) {
				fprintf(fp, "pmu: %s event%d: %s umask%d: %s :: invalid grpid %d (must be < %d)\n", pmu->name, i, pe[i].name, j, pe[i].umasks[j].uname, pe[i].umasks[j].grpid, pe[i].ngrp);
				error++;
			}
			if (pe[i].ngrp > 1 && (!pe[i].umasks[j].grpmsk || pe[i].umasks[j].grpmsk > 0xff)) {
				fprintf(fp, "pmu: %s event%d: %s umask%d: %s :: invalid grmsk=0x%x\n", pmu->name, i, pe[i].name, j, pe[i].umasks[j].uname, pe[i].umasks[j].grpmsk);
				error++;
			}
		}

		/* heck for excess unit masks */
		for(; j < INTEL_X86_NUM_UMASKS; j++) {
			if (pe[i].umasks[j].uname || pe[i].umasks[j].udesc) {
				fprintf(fp, "pmu: %s event%d: %s :: numasks (%d) invalid more events exists\n", pmu->name, i, pe[i].name, pe[i].numasks);
				error++;
			}
		}

		if (pe[i].flags & INTEL_X86_NCOMBO) {
			fprintf(fp, "pmu: %s event%d: %s :: NCOMBO is unit mask only flag\n", pmu->name, i, pe[i].name);
			error++;
		}

		for(j=0; j < pe[i].numasks; j++) {

			if (pe[i].umasks[j].ufrom)
				continue;

			if (pe[i].umasks[j].uflags & INTEL_X86_NCOMBO)
				continue;

			for(k=j+1; k < pe[i].numasks; k++) {
				if (pe[i].umasks[k].ufrom)
					continue;
				if (pe[i].umasks[k].uflags & INTEL_X86_NCOMBO)
					continue;
				if (pe[i].umasks[k].grpid != pe[i].umasks[j].grpid)
					continue;
				if ((pe[i].umasks[j].ucode &  pe[i].umasks[k].ucode)) {
					fprintf(fp, "pmu: %s event%d: %s :: umask %s and %s have overlapping code bits\n", pmu->name, i, pe[i].name, pe[i].umasks[j].uname, pe[i].umasks[k].uname);
					error++;
				}
			}
		}
	}
	return error ? PFM_ERR_INVAL : PFM_SUCCESS;
}

int pfm_intel_x86_get_event_attr_info(void *this, int idx, int attr_idx, pfmlib_attr_info_t *info)
{
	const intel_x86_entry_t *pe = this_pe(this);
	int ret;

	if (attr_idx > pe[idx].numasks)
		return 0;

	ret = !!(pe[idx].umasks[attr_idx].uflags & INTEL_X86_DFL);
	if (ret)
		info->dfl_val64 = 0;
	return ret;
}
