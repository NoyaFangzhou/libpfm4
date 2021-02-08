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
#include <assert.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/version.h>

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

struct ListNode * root_msb;
pthread_mutex_t msb_lock = PTHREAD_MUTEX_INITIALIZER;

static void
sigio_handler(int n, siginfo_t *info, struct sigcontext *sc)
{
	if (info->si_code == POLL_HUP) {
		// printf("sigio received\n");
		int id, ret;


		id = perf_fd2event(fds, num_fds, info->si_fd);
		if (id == -1)
			errx(1, "no event associated with fd=%d", info->si_fd);

		struct perf_event_mmap_page * metadata_page = fds[id].buf;
		struct mem_sampling_backed *new_msb = (struct mem_sampling_backed *) malloc(sizeof(struct mem_sampling_backed));
		if (new_msb == NULL) {
			errx(1, "could not malloc mem_sampling_backed\n");
		}
		new_msb->fd = info->si_fd;
		ret = handler_read_ring_buffer(metadata_page, new_msb);
		if (new_msb->buffer == NULL) {
			errx(1, "failed to read the perf_event_mmap_page ring buffer\n");
		}
		pthread_mutex_lock(&msb_lock);
		list_put(&root_msb, (void *)new_msb, sizeof(struct mem_sampling_backed));
		// can print msb. It stores all samples
		pthread_mutex_unlock(&msb_lock);
		// printf("Notification:%lu ", notification_received);
	#if 0
		ret = perf_read_buffer(fds+id, &ehdr, sizeof(ehdr));
		if (ret)
			errx(1, "cannot read event header");

		if (ehdr.type != PERF_RECORD_SAMPLE) {
			warnx("unexpected sample type=%d, skipping\n", ehdr.type);
			perf_skip_buffer(fds+id, ehdr.size);
			goto skip;
		}
		
		ret = perf_display_sample(fds, num_fds, 0, &ehdr, stdout);
	#endif
		/*
		 * increment our notification counter
		 */
		notification_received++;
	// skip:
		/*
	 	 * rearm the counter for one more shot
	 	 */
		ret = ioctl(info->si_fd, PERF_EVENT_IOC_REFRESH, SMPL_PERIOD);
		if (ret == -1)
			err(1, "cannot refresh");
	}
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
	options.events[0] = "MEM_UOPS_RETIRED:ALL_STORES";
	if (!argv[optind])
		errx(1, "you must specify a command to execute\n");


	char **arg;
	arg = argv+optind;

	struct sigaction act;
	// uint64_t *val;
	size_t pgsz;
	int ret, i;

	ret = pfm_initialize();
	if (ret != PFM_SUCCESS)
		errx(1, "Cannot initialize library: %s", pfm_strerror(ret));

	pgsz = (size_t)sysconf(_SC_PAGESIZE);

	/*
	 * Install the signal handler (SIGIO)
	 */
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_sigaction = (void *)sigio_handler;
	act.sa_flags = SA_SIGINFO;
	ret = sigaction (SIGIO, &act, NULL);
	if (ret < 0)
		err(1, "could not set up signal handler\n");

	ret = sample_parser_init();
	if (ret < 0)
		errx(1, "Cannot malloc struct mem_sampling_stat\n");
#if 0
	sigset_t new, old;
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
#endif
	/*
 	 * allocates fd for us
 	 */
 	ret = perf_setup_list_events(
 						"MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3", &fds, &num_fds);
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
	printf("attach pfm on pid: %d\n", pid);




	fds[0].fd = -1;
	for(i=0; i < num_fds; i++) {

		/* want a notification for every each added to the buffer */
		fds[i].hw.disabled = !i;
		fds[i].hw.wakeup_events = 1;
		// fds[i].hw.enable_on_exec = 1;
		fds[i].hw.sample_type = PERF_SAMPLE_IP|PERF_SAMPLE_ADDR|PERF_SAMPLE_WEIGHT|PERF_SAMPLE_DATA_SRC;
		fds[i].hw.sample_period = SMPL_PERIOD;
		fds[i].hw.exclude_kernel = 1;
		fds[i].hw.exclude_hv = 1;
		fds[i].hw.mmap = 1;
		fds[i].hw.task = 1;
		fds[i].hw.precise_ip = 2; // have 0 skid
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,1,0)
		fds[i].hw.use_clockid=1;
		fds[i].hw.clockid = CLOCK_MONOTONIC_RAW;
#endif
		/* read() returns event identification for signal handler */
		// fds[i].hw.read_format = PERF_FORMAT_GROUP|PERF_FORMAT_ID|PERF_FORMAT_SCALE;
		fds[i].fd = perf_event_open(&fds[i].hw, pid, -1, fds[0].fd, 0);
		if (fds[i].fd == -1)
			err(1, "cannot attach event %s", fds[i].name);
	}
#if 0
	sz = (3+2*num_fds)*sizeof(uint64_t);
	val = malloc(sz);
	if (!val)
		err(1, "cannot allocated memory");
	/*
	 * On overflow, the non lead events are stored in the sample.
	 * However we need some key to figure the order in which they
	 * were laid out in the buffer. The file descriptor does not
	 * work for this. Instead, we extract a unique ID for each event.
	 * That id will be part of the sample for each event value.
	 * Therefore we will be able to match value to events
	 *
	 * PERF_FORMAT_ID: returns unique 64-bit identifier in addition
	 * to event value.
	 */
	if (fds[0].fd == -1)
		errx(1, "cannot create event 0");

	ret = read(fds[0].fd, val, sz);
	if (ret == -1)
		err(1, "cannot read id %zu", sizeof(val));

	/*
	 * we are using PERF_FORMAT_GROUP, therefore the structure
	 * of val is as follows:
	 *
	 *      { u64           nr;
	 *        { u64         time_enabled; } && PERF_FORMAT_ENABLED
	 *        { u64         time_running; } && PERF_FORMAT_RUNNING
	 *        { u64         value;                  
	 *          { u64       id;           } && PERF_FORMAT_ID
	 *        }             cntr[nr];               
	 * We are skipping the first 3 values (nr, time_enabled, time_running)
	 * and then for each event we get a pair of values.
	 */ 
	for(i=0; i < num_fds; i++) {
		fds[i].id = val[2*i+1+3];
		printf("%"PRIu64"  %s\n", fds[i].id, fds[i].name);
	}
#endif

	for (i=0; i < num_fds; i++) {
	 
		fds[i].buf = mmap(NULL, (buffer_pages+1)*pgsz, PROT_READ|PROT_WRITE, MAP_SHARED, fds[i].fd, 0);

		if (fds[i].buf == MAP_FAILED)
			err(1, "cannot mmap buffer");
		
		fds[i].pgmsk = (buffer_pages * pgsz) - 1;

		ret = ioctl(fds[i].fd, PERF_EVENT_IOC_RESET, 0);
		if (ret == -1)
			err(1, "cannot reset");
		/*
		 * setup asynchronous notification on the file descriptor
		 */
		ret = fcntl(fds[i].fd, F_SETFL, fcntl(fds[i].fd, F_GETFL, 0) | O_ASYNC);
		if (ret == -1)
			err(1, "cannot set ASYNC");

		/*
	 	 * necessary if we want to get the file descriptor for
	 	 * which the SIGIO is sent in siginfo->si_fd.
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

		/*
		 * enable the group for one period
		 */
		ret = ioctl(fds[i].fd, PERF_EVENT_IOC_REFRESH , SMPL_PERIOD);
		if (ret == -1)
			errx(1, "cannot refresh fds[%d]", i);

		ret = ioctl(fds[i].fd, PERF_EVENT_IOC_ENABLE, 0);
		if (ret == -1)
			errx(1, "cannot enable fds[%d]", i);
	}

#if 0
	fds[0].fd = -1;
	for(i=0; i < num_fds; i++) {
		/* want a notification for each sample added to the buffer */
		fds[i].hw.disabled =  !!i;
		printf("i=%d disabled=%d\n", i, fds[i].hw.disabled);
		if (!i) {
			fds[i].hw.wakeup_events = 1;
			fds[i].hw.enable_on_exec = 1;
			fds[i].hw.exclude_kernel = 1;
			fds[i].hw.sample_type = PERF_SAMPLE_IP|PERF_SAMPLE_ADDR|PERF_SAMPLE_READ|PERF_SAMPLE_WEIGHT|PERF_SAMPLE_DATA_SRC|PERF_SAMPLE_PERIOD;
			fds[i].hw.sample_period = SMPL_PERIOD;
			/* read() returns event identification for signal handler */
			fds[i].hw.read_format = PERF_FORMAT_GROUP|PERF_FORMAT_ID|PERF_FORMAT_SCALE;
		}
		fds[i].fd = perf_event_open(&fds[i].hw, pid, -1, fds[0].fd, 0);
		if (fds[i].fd == -1) {
			warn("cannot attach event %s", fds[i].name);
			goto error;
		}
	}

	sz = (3+2*num_fds)*sizeof(uint64_t);
	val = malloc(sz);
	if (!val)
		err(1, "cannot allocated memory");
	/*
	 * On overflow, the non lead events are stored in the sample.
	 * However we need some key to figure the order in which they
	 * were laid out in the buffer. The file descriptor does not
	 * work for this. Instead, we extract a unique ID for each event.
	 * That id will be part of the sample for each event value.
	 * Therefore we will be able to match value to events
	 *
	 * PERF_FORMAT_ID: returns unique 64-bit identifier in addition
	 * to event value.
	 */
	if (fds[0].fd == -1)
		errx(1, "cannot create event 0");

	ret = read(fds[0].fd, val, sz);
	if (ret == -1)
		err(1, "cannot read id %zu", sizeof(val));

	fds[0].buf = mmap(NULL, (buffer_pages + 1)*pgsz, PROT_READ|PROT_WRITE, MAP_SHARED, fds[0].fd, 0);
	if (fds[0].buf == MAP_FAILED)
		err(1, "cannot mmap buffer");
	/*
	 * setup asynchronous notification on the file descriptor
	 */
	ret = fcntl(fds[0].fd, F_SETFL, fcntl(fds[0].fd, F_GETFL, 0) | O_ASYNC);
	if (ret == -1)
		err(1, "cannot set ASYNC");

	/*
	 * necessary if we want to get the file descriptor for
	 * which the SIGIO is sent for in siginfo->si_fd.
	 * SA_SIGINFO in itself is not enough
	 */
	ret = fcntl(fds[0].fd, F_SETSIG, SIGIO);
	if (ret == -1)
		err(1, "cannot setsig");

	/*
	 * get ownership of the descriptor
	 */
	ret = fcntl(fds[0].fd, F_SETOWN, getpid());
	if (ret == -1)
		err(1, "cannot setown");

	fds[0].pgmsk = (buffer_pages * pgsz) - 1;

	ret = ioctl(fds[0].fd, PERF_EVENT_IOC_REFRESH , 1);
	if (ret == -1)
		err(1, "cannot refresh");
#endif

	/* Start the child process */
	if (go[1] > -1)
		close(go[1]);

	// busyloop();
	while(waitpid(pid, &status, WNOHANG) == 0);

	for (i = 0; i < num_fds; i++) {
		ret = ioctl(fds[i].fd, PERF_EVENT_IOC_DISABLE, 1);
		if (ret == -1)
			errx(1, "cannot disable fd[%d]", i);
	}

// error:
	// while(waitpid(pid, &status, WNOHANG) == 0);
	for (i = 0; i < num_fds; i++) {
		// dump_all_samples(fds[i].fd, root_msb);
		collect_sampling_stat(fds[i].fd, root_msb);
	}
	dump_memory_region();
	dump_sample_statistics();
	clean_up_sample_parser(root_msb);
	/*
	 * destroy our session
	 */
	for(i=0; i < num_fds; i++)
		if (fds[i].fd > -1)
			close(fds[i].fd);

	perf_free_fds(fds, num_fds);

	/* free libpfm resources cleanly */
	pfm_terminate();

	printf("total notification received: %lu\n", notification_received);

	assert(notification_received == (uint64_t)list_length(root_msb));
	return 0;
}
