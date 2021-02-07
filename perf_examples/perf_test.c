/*
 * perf_test.c - get a command to execute and sample multuiple events
 * in one group
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
 */
#include <sys/types.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <locale.h>
#include <fcntl.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "perf_util.h"
#include "sample_parser.h"

#define MAX_GROUPS	256
#define MAX_CPUS	64
#define SMPL_PERIOD	1000ULL

typedef struct {
	const char *events[MAX_GROUPS];
	pid_t pid;
} options_t;

static volatile unsigned long notification_received;

static perf_event_desc_t *fds;
static int num_fds;
static options_t options;

static int buffer_pages = 64; /* size of buffer payload  (must be power of 2) */

static void
sigio_handler(int n, siginfo_t *info, struct sigcontext *sc)
{
	printf("sigio received\n");
	struct perf_event_header ehdr;
	int id, ret;
	
	id = perf_fd2event(fds, num_fds, info->si_fd);
	if (id == -1)
		errx(1, "cannot find event for descriptor %d", info->si_fd);

	ret = perf_read_buffer(fds+id, &ehdr, sizeof(ehdr));
	if (ret)
		errx(1, "cannot read event header");

	if (ehdr.type != PERF_RECORD_SAMPLE) {
		warnx("unknown event type %d, skipping", ehdr.type);
		perf_skip_buffer(fds+id, ehdr.size - sizeof(ehdr));
		goto skip;
	}
	sample_t *sample = (sample_t *)((char *)(&ehdr) + 8);
	notification_received++;
	display_sample_data(sample);
	ret = perf_display_sample(fds, num_fds, 0, &ehdr, stdout);

skip:
	/*
 	 * rearm the counter for one more shot
 	 */
	ret = ioctl(info->si_fd, PERF_EVENT_IOC_REFRESH, 1);
	if (ret == -1)
		err(1, "cannot refresh");

}

/*
 * infinite loop waiting for notification to get out
 */
void
busyloop(void)
{
	/*
	 * busy loop to burn CPU cycles
	 */
	for(;notification_received < 10;) ;
}

int
child(char **arg)
{
	/*
	 * execute the requested command
	 */
	execvp(arg[0], arg);
	errx(1, "cannot exec: %s\n", arg[0]);
	/* not reached */
}

static void
usage(void)
{
	printf("usage: task_cpu [-h] [-p pid] [-e event1,event2,...] cmd\n"
		"-h\t\tget help\n"
		"-e ev,ev\tgroup of events to measure (multiple -e switches are allowed)\n"
		);
}

int
main(int argc, char **argv)
{
	/*
		Read command line
	 */
	int c;

	setlocale(LC_ALL, "");

	while ((c=getopt(argc, argv,"+he:p:")) != -1) {
		switch(c) {
			case 'e':
				options.events[0] = optarg;
				break;
			case 'p':
				options.pid = atoi(optarg);
				break;
			case 'h':
				usage();
				exit(0);
			default:
				errx(1, "unknown error");
		}
	}
	options.events[0] = "MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3,MEM_UOPS_RETIRED:ALL_STORES";
	if (!argv[optind])
		errx(1, "you must specify a command to execute\n");


	char **arg;
	arg = argv+optind;

	struct sigaction act;
	sigset_t new, old;
	size_t pgsz;
	int ret, i;

	ret = pfm_initialize();
	if (ret != PFM_SUCCESS)
		errx(1, "Cannot initialize library: %s", pfm_strerror(ret));

	pgsz = sysconf(_SC_PAGESIZE);

	/*
	 * Install the signal handler (SIGIO)
	 */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = (void *)sigio_handler;
	act.sa_flags = SA_SIGINFO;
	sigaction (SIGIO, &act, 0);

	sigemptyset(&old);
	sigemptyset(&new);
	sigaddset(&new, SIGIO);

	ret = sigprocmask(SIG_SETMASK, NULL, &old);
	if (ret)
		err(1, "sigprocmask failed");

	if (sigismember(&old, SIGIO)) {
		warnx("program started with SIGIO masked, unmasking it now\n");
		ret = sigprocmask(SIG_UNBLOCK, &new, NULL);
		if (ret)
			err(1, "sigprocmask failed");
	}

	/*
 	 * allocates fd for us
 	 */
	ret = perf_setup_list_events(options.events[0],
				        &fds, &num_fds);
	if (ret || !num_fds)
		exit(1);

	/*
		Create child process running the given command
	 */
	int ready[2], go[2], status;
	char buf;
	pid_t pid;
	go[0] = go[1] = -1;
	ret = pipe(ready);
	if (ret)
		err(1, "cannot create pipe ready");

	ret = pipe(go);
	if (ret)
		err(1, "cannot create pipe go");


	/*
	 * Create the child task
	 */
	printf("creating child process using fork()\n");
	if ((pid=fork()) == -1)
		err(1, "Cannot fork process");

	/*
	 * and launch the child code
	 *
	 * The pipe is used to avoid a race condition
	 * between fork() and exec(). We need the pid
	 * of the new task but we want to start measuring
	 * at the first user level instruction. Thus we
	 * need to prevent exec until we have attached
	 * the events.
	 */
	if (pid == 0) {
		close(ready[0]);
		close(go[1]);

		/*
		 * let the parent know we exist
		 */
		
		close(ready[1]);
		if (read(go[0], &buf, 1) == -1)
			err(1, "unable to read go_pipe");
		printf("executing child process %s\n", arg[0]);
		exit(child(arg));
	}

	close(ready[1]);
	close(go[0]);

	if (read(ready[0], &buf, 1) == -1)
		err(1, "unable to read child_ready_pipe");

	close(ready[0]);


	/*
		Config fds for sampling
	 */
	printf("attach pfm on pid: %d\n", options.pid);
	fds[0].fd = -1;
	for(i=0; i < num_fds; i++) {
		/* want a notification for each sample added to the buffer */
		fds[i].hw.disabled =  !!i;
		printf("i=%d disabled=%d\n", i, fds[i].hw.disabled);
		fds[i].hw.wakeup_events = 1;
		fds[i].hw.enable_on_exec = 1;
		fds[i].hw.exclude_kernel = 1;
		fds[i].hw.sample_type = PERF_SAMPLE_IP|PERF_SAMPLE_ADDR|PERF_SAMPLE_DATA_SRC;
		fds[i].hw.sample_period = SMPL_PERIOD;

		fds[i].fd = perf_event_open(&fds[i].hw, options.pid, -1, fds[0].fd, 0);
		if (fds[i].fd == -1) {
			warn("cannot attach event %s", fds[i].name);
			goto error;
		}

		fds[i].buf = mmap(NULL, (buffer_pages + 1)*pgsz, PROT_READ|PROT_WRITE, MAP_SHARED, fds[i].fd, 0);
		if (fds[i].buf == MAP_FAILED)
			err(1, "cannot mmap buffer");
		/*
		 * setup asynchronous notification on the file descriptor
		 */
		ret = fcntl(fds[i].fd, F_SETFL, fcntl(fds[i].fd, F_GETFL, 0) | O_ASYNC);
		if (ret == -1)
			err(1, "cannot set ASYNC");

		/*
		 * necessary if we want to get the file descriptor for
		 * which the SIGIO is sent for in siginfo->si_fd.
		 * SA_SIGINFO in itself is not enough
		 */
		ret = fcntl(fds[i].fd, F_SETSIG, SIGIO);
		if (ret == -1)
			err(1, "cannot setsig");

		/*
		 * get ownership of the descriptor
		 */
		ret = fcntl(fds[i].fd, F_SETOWN, getpid());
		if (ret == -1)
			err(1, "cannot setown");

		fds[i].pgmsk = (buffer_pages * pgsz) - 1;
	}

	for(i=0; i < num_fds; i++) {
		ret = ioctl(fds[i].fd, PERF_EVENT_IOC_REFRESH , 1);
		if (ret == -1)
			err(1, "cannot refresh");
	}

	/* Start the child process */
	if (go[1] > -1)
		close(go[1]);

	// busyloop();
	while(waitpid(pid, &status, WNOHANG) == 0);

	prctl(PR_TASK_PERF_EVENTS_DISABLE);

error:
	while(waitpid(pid, &status, WNOHANG) == 0);
	/*
	 * destroy our session
	 */
	for(i=0; i < num_fds; i++)
		if (fds[i].fd > -1)
			close(fds[i].fd);

	perf_free_fds(fds, num_fds);

	/* free libpfm resources cleanly */
	pfm_terminate();

	return 0;
}
