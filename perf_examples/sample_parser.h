#ifndef __SAMPLE_PARSER_H__
#define __SAMPLE_PARSER__
#include <stdlib.h>
#include <string.h>
#include <perfmon/pfmlib_perf_event.h>


typedef struct __attribute__ ((__packed__)) {
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
} sample_t;

void read_sample_data(perf_event_mmap_page *metadata, void * buff, size_t * sample_size);
void display_sample_data(sample_t * sample);

#endif