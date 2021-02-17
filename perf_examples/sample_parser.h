#ifndef __SAMPLE_PARSER_H__
#define __SAMPLE_PARSER__
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <linux/kernel.h> 
#include <perfmon/pfmlib_perf_event.h>

#include "perf_util.h"
#include "linked_list.h"

#define MEMORY_LOAD_EVENT		"MEM_TRANS_RETIRED:LOAD_LATENCY:ldlat=3"
#define MEMORY_STORE_EVENT		"MEM_UOPS_RETIRED:ALL_STORES"

#define rmb()           asm volatile("lfence" ::: "memory")

struct __attribute__ ((__packed__)) sample_t {
	uint64_t	ip;						// IP of the sampled instruction
	uint64_t	addr;					// Addr of the sampled instruction
	uint64_t	weight;					// a 64-bit value provided by the hardware is recorded that indicates how costly the event was.
	/*
		struct of perf_mem_data_src union 

		union perf_mem_data_src {
			__u64 val;
			struct {
				__u64   mem_op:5,	 	type of opcode 
						mem_lvl:14,	 	memory hierarchy level 
						mem_snoop:5,	snoop mode 
						mem_lock:2,	 	lock instr 
						mem_dtlb:7,	 	tlb access 
						mem_rsvd:31;
			};
		};
	 */
	union perf_mem_data_src data_src;	// a  64-bit value is recorded that is made up of the following fields: 
};

struct mem_sampling_backed {
	int fd;									/* file descriptor */
	void *buffer;							/* data region */
	size_t sz;								/* size of the buffer */
};

struct address_region {
	uint64_t addr_start;
	uint64_t addr_end;
};

struct mem_sampling_stat {
	uint64_t consumed;
	uint64_t na_miss_count;
	uint64_t cache1_count;
	uint64_t cache2_count;
	uint64_t cache3_count;
	uint64_t lfb_count;
	uint64_t memory_count;
	uint64_t remote_memory_count;
	uint64_t remote_cache_count;
	uint64_t total_count;
};

int sample_parser_init();
int handler_read_ring_buffer(struct perf_event_mmap_page *metadata_page, struct mem_sampling_backed *new_msb);
int read_sample_data(struct perf_event_mmap_page *metadata, void * buff, size_t * sample_size);
/* build memory regions */
void build_page_region();
/* collect sampling stat */
int collect_sampling_stat(int fd, struct ListNode * root_msb);

<<<<<<< HEAD
/* display method */
void dump_addresses();
void dump_memory_region();
int display_single_msb(int fd, struct ListNode * msb_node);
void dump_all_samples(int fd, struct ListNode * root_msb);
void dump_sample_statistics();
/* cleanup */
void clean_up_sample_parser(struct ListNode * root_msb);
=======
void read_sample_data(perf_event_mmap_page *metadata, void * buff, size_t * sample_size);
void display_sample_data(sample_t * sample);
>>>>>>> 2df1a3f021022621c6ece4cd5a72c09b549f4f36

#endif