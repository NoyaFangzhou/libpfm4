#include "sample_parser.h"



// local function declaration region
char* concat(const char *s1, const char *s2);

int is_served_by_local_cache1(union perf_mem_data_src data_src);
int is_served_by_local_cache2(union perf_mem_data_src data_src);
int is_served_by_local_cache3(union perf_mem_data_src data_src);
int is_served_by_local_lfb(union perf_mem_data_src data_src);
int is_served_by_local_cache(union perf_mem_data_src data_src);
int is_served_by_local_memory(union perf_mem_data_src data_src);
int is_served_by_remote_cache_or_local_memory(union perf_mem_data_src data_src);
int is_served_by_remote_memory(union perf_mem_data_src data_src);
int is_served_by_local_NA_miss(union perf_mem_data_src data_src);
char *get_data_src_opcode(union perf_mem_data_src data_src);
char *get_data_src_level(union perf_mem_data_src data_src);

/**
 * Helper function
 * @param  s1 [description]
 * @param  s2 [description]
 * @return    [description]
 */
char* concat(const char *s1, const char *s2) 
{
	char *result = malloc(strlen(s1) + strlen(s2) + 1);
	if (result == NULL) {
		return "malloc failed in concat\n";
	}
	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

int is_served_by_local_cache1(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		if (data_src.mem_lvl & PERF_MEM_LVL_L1) {
			return 1;
		}
	}
	return 0;
}

int is_served_by_local_cache2(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		if (data_src.mem_lvl & PERF_MEM_LVL_L2) {
			return 1;
		}
	}
	return 0;
}

int is_served_by_local_cache3(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		if (data_src.mem_lvl & PERF_MEM_LVL_L3) {
			return 1;
		}
	}
	return 0;
}

int is_served_by_local_lfb(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		if (data_src.mem_lvl & PERF_MEM_LVL_LFB) {
			return 1;
		}
	}
	return 0;
}


int is_served_by_local_cache(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		if (data_src.mem_lvl & PERF_MEM_LVL_L1) {
			return 1;
		}
		if (data_src.mem_lvl & PERF_MEM_LVL_LFB) {
			return 1;
		}
		if (data_src.mem_lvl & PERF_MEM_LVL_L2) {
			return 1;
		}
		if (data_src.mem_lvl & PERF_MEM_LVL_L3) {
			return 1;
		}
	}
	return 0;
}

int is_served_by_local_memory(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
	    if (data_src.mem_lvl & PERF_MEM_LVL_LOC_RAM) {
	    	return 1;
	    }
	}
  return 0;
}

int is_served_by_remote_cache_or_local_memory(union perf_mem_data_src data_src)
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT && data_src.mem_lvl & PERF_MEM_LVL_REM_CCE1) {
		return 1;
	}
	return 0;
}

int is_served_by_remote_memory(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM1) {
			return 1;
		} else if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM2) {
			return 1;
		}
	}
	return 0;
}

int is_served_by_local_NA_miss(union perf_mem_data_src data_src) 
{
	if (data_src.mem_lvl & PERF_MEM_LVL_NA) {
		return 1;
	}
	if (data_src.mem_lvl & PERF_MEM_LVL_MISS && data_src.mem_lvl & PERF_MEM_LVL_L3) {
		return 1;
	}
	return 0;
}

char *get_data_src_opcode(union perf_mem_data_src data_src) {
	char * res = concat("", "");
	char * old_res;
	if (data_src.mem_op & PERF_MEM_OP_NA) {
		old_res = res;
		res = concat(res, "NA");
		free(old_res);
	}
	if (data_src.mem_op & PERF_MEM_OP_LOAD) {
		old_res = res;
		res = concat(res, "Load");
		free(old_res);
	}
	if (data_src.mem_op & PERF_MEM_OP_STORE) {
		old_res = res;
		res = concat(res, "Store");
		free(old_res);
	}
	if (data_src.mem_op & PERF_MEM_OP_PFETCH) {
		old_res = res;
		res = concat(res, "Prefetch");
		free(old_res);
	}
	if (data_src.mem_op & PERF_MEM_OP_EXEC) {
		old_res = res;
		res = concat(res, "Exec code");
		free(old_res);
	}
	return res;
}

char *get_data_src_level(union perf_mem_data_src data_src) 
{
	char * res = concat("", "");
	char * old_res;
	if (data_src.mem_lvl & PERF_MEM_LVL_NA) {
		old_res = res;
		res = concat(res, "NA");
		free(old_res);
	}
	if (data_src.mem_lvl & PERF_MEM_LVL_L1) {
		old_res = res;
		res = concat(res, "L1");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_LFB) {
		old_res = res;
		res = concat(res, "LFB");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_L2) {
		old_res = res;
		res = concat(res, "L2");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_L3) {
		old_res = res;
		res = concat(res, "L3");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_LOC_RAM) {
		old_res = res;
		res = concat(res, "Local_RAM");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM1) {
		old_res = res;
		res = concat(res, "Remote_RAM_1_hop");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_REM_RAM2) {
		old_res = res;
		res = concat(res, "Remote_RAM_2_hops");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_REM_CCE1) {
		old_res = res;
		res = concat(res, "Remote_Cache_1_hop");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_REM_CCE2) {
		old_res = res;
		res = concat(res, "Remote_Cache_2_hops");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_IO) {
		old_res = res;
		res = concat(res, "I/O_Memory");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_UNC) {
		old_res = res;
		res = concat(res, "Uncached_Memory");
		free(old_res);
	}
	if (data_src.mem_lvl & PERF_MEM_LVL_HIT) {
		old_res = res;
		res = concat(res, "_Hit");
		free(old_res);
	} else if (data_src.mem_lvl & PERF_MEM_LVL_MISS) {
		old_res = res;
		res = concat(res, "_Miss");
		free(old_res);
	}
	return res;
}

void display_sample_data(sample_t * sample)
{
	// if (is_served_by_local_NA_miss(sample->data_src)) {
	// 	na_miss_count++;
 //    }
 //    if (is_served_by_local_cache1(sample->data_src)) {
	// 	cache1_count++;
 //    }
 //    if (is_served_by_local_cache2(sample->data_src)) {
	// 	cache2_count++;
 //    }
 //    if (is_served_by_local_cache3(sample->data_src)) {
	// 	cache3_count++;
 //    }
 //    if (is_served_by_local_lfb(sample->data_src)) {
	// 	lfb_count++;
 //    }
 //    if (is_served_by_local_memory(sample->data_src)) {
	// 	memory_count++;
 //    }
 //    if (is_served_by_remote_memory(sample->data_src)) {
 //    	remote_memory_count++;
 //    }
 //    if (is_served_by_remote_cache_or_local_memory(sample->data_src)) {
 //    	remote_cache_count++;
 //    }
 //    total_count++;
    printf("pc=%" PRIx64 ", @=%" PRIx64 ", src level=%s, latency=%" PRIu64 "\n", sample->ip, sample->addr, get_data_src_level(sample->data_src), sample->weight);
}

/**
 * The sample is composed by 2 part, 1 page metadata + 2^n length ring buffer
 * First we need to know the size of this sample by parsing the field in the 
 * metadata field, then we need to parse the data region
 * @param metadata [description]
 */
void read_sample_data(perf_event_mmap_page *metadata, void * buff, size_t * sample_size)
{
	// parse the metadata
	uint64_t head, tail;
	head = metadata->data_head;
	rmb(); // told the compiler not reorder these two instructions
	tail = metadata->data_tail;

	if (head > tail) {
		*sample_size = head - tail;
	} else {
		*sample_size = (metadata->data_size - tail) + head;
	}

	// parse the data region
	buff = (void *)malloc(*sample_size);
	uint8_t * data_start_addr = (uint8_t *)metadata;
	/* 
		Here we assume Linux kernel version >= 4.1.0, data_offset is available on if kernel version >= 4.1.0. Otherwise, this will raise an compilation error

		For kernel version less than 4.1.0, use the following code 
		data_start_addr += (size_t)sysconf(_SC_PAGESIZE);
	*/
	data_start_addr += metadata->data_offset;

	if (head > tail) {
		memcpy(buff, data_start_addr+tail, *sample_size);
	} else {
		memcpy(buff, data_start_addr+tail, (metadata->data_size - tail));
		memcpy((char*)buf + (metadata->data_size - tail), data_start_addr, head);
	}
	metadata->data_tail = head;
}