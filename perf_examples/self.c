/*
 * self.c - example of a simple self monitoring task
 *
 * Copyright (c) 2009 Google, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * Based on:
 * Copyright (c) 2002-2007 Hewlett-Packard Development Company, L.P.
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
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
 * This file is part of libpfm, a performance monitoring support library for
 * applications on Linux.
 */

#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#include <perfmon/pfmlib_perf_event.h>
#include "perf_util.h"

static char *gen_events[]={
	"PERF_COUNT_HW_CPU_CYCLES",
	"PERF_COUNT_HW_INSTRUCTIONS",
	NULL
};

static volatile int quit;
void sig_handler(int n)
{
	quit = 1;
}

void
noploop(void)
{
	for(;quit == 0;);
}

int
main(int argc, char **argv)
{
	perf_event_desc_t *fds;
	uint64_t values[3];
	int i, ret, num;

	/*
	 * Initialize pfm library (required before we can use it)
	 */
	ret = pfm_initialize();
	if (ret != PFM_SUCCESS)
		errx(1, "Cannot initialize library: %s", pfm_strerror(ret));

	num = perf_setup_argv_events(argc > 1 ? argv+1 : gen_events, &fds);
	if (num == -1)
		errx(1, "cannot setup events");

	fds[0].fd = -1;
	for(i=0; i < num; i++) {
		/* request timing information necessary for scaling */
		fds[i].hw.read_format = PERF_FORMAT_SCALE;

		fds[i].hw.disabled = 1; /* do not start now */

		/* each event is in an independent group (multiplexing likely) */
		fds[i].fd = perf_event_open(&fds[i].hw, 0, -1, -1, 0);
		if (fds[i].fd == -1)
			err(1, "cannot open event %d", i);
	}

	signal(SIGALRM, sig_handler);

	/*
 	 * enable all counters attached to this thread and created by it
 	 */
	ret = prctl(PR_TASK_PERF_EVENTS_ENABLE);
	if (ret)
		err(1, "prctl(enable) failed");

	alarm(10);

	noploop();

	/*
 	 * disable all counters attached to this thread
 	 */
	ret = prctl(PR_TASK_PERF_EVENTS_DISABLE);
	if (ret)
		err(1, "prctl(disable) failed");

	/*
	 * now read the results. We use pfp_event_count because
	 * libpfm guarantees that counters for the events always
	 * come first.
	 */
	memset(values, 0, sizeof(values));

	for (i=0; i < num; i++) {
		uint64_t val;
		double ratio;

		ret = read(fds[i].fd, values, sizeof(values));
		if (ret < sizeof(values)) {
			if (ret == -1)
				err(1, "cannot read results: %s", strerror(errno));
			else
				warnx("could not read event%d", i);
		}
		/*
		 * scaling is systematic because we may be sharing the PMU and
		 * thus may be multiplexed
		 */
		val = perf_scale(values);
		ratio = perf_scale_ratio(values);

		if (ratio == 1.0)
			printf("%20"PRIu64" %s\n", val, fds[i].name);
		else
			if (ratio == 0.0)
				printf("%20"PRIu64" %s (did not run: competing session)\n", val, fds[i].name);
			else
				printf("%20"PRIu64" %s (scaled from %.2f%% of time)\n", val, fds[i].name, ratio*100.0);

		close(fds[i].fd);
	}
	free(fds);
	return 0;
}
