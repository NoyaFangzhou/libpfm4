
#ifndef __KERNEL_UTIL_H__
#define __KERNEL_UTIL_H__
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <numaif.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <sys/types.h>

/* signal macro */
#define SIGNAL_PAUSE	0
#define SIGNAL_CONTINUE	1

/* check the page */
int kernel_query_node(pid_t pid, unsigned long va);

long kernel_move_single_page(pid_t pid, unsigned long va, const int target_node);

long kernel_move_pages(pid_t pid, unsigned long count, void * pages, const int target_node, void * failed);

int kernel_send_signal(pid_t pid, int type);

#endif