#include "sample_parser.h"
#include <err.h>

struct mem_sampling_stat * stat;
struct ListNode * root_address;
struct ListNode * root_region;

// ----------------------------------------------------------------------------
// local function declaration region
// ----------------------------------------------------------------------------
void dump_single_msb(struct ListNode * node);
void dump_single_address(struct ListNode * node);
void dump_single_region(struct ListNode * node);
int address_comp(void * a, void * b);
void clean_msb(struct ListNode * node);
void clean_address(struct ListNode * node);
void clean_region(struct ListNode * node);

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

void display_sample_data(struct sample_t * sample);
/* ------------------------------------------------------------------------- */



// ----------------------------------------------------------------------------
// Struct node util
// ----------------------------------------------------------------------------
// value print
void dump_single_msb(struct ListNode * node)
{
	if (node->val == NULL)
		return;
	struct mem_sampling_backed * msb = (struct mem_sampling_backed *)node->val;
	struct perf_event_header *header;
	if (msb != NULL) {
		header  = (struct perf_event_header *)(msb->buffer);
		size_t ehdr_offset = 0;
		while (ehdr_offset < msb->sz) {
			if (header->size == 0) {
				err(1, "invalid header size = 0\n");
			}
			if (header -> type == PERF_RECORD_SAMPLE) {
				struct sample_t *sample = (struct sample_t *)((char *)(header) + 8);
				display_sample_data(sample);
			}
			ehdr_offset += header->size;
			header = (struct perf_event_header *)((char*)msb->buffer+ehdr_offset);
		}
	}
	printf("\n");
}

void dump_single_address(struct ListNode * node)
{
	if (node->val != NULL) {
        uint64_t val = *((uint64_t *)(node->val));
        printf("%lu", val);
    }
}

void dump_single_region(struct ListNode * node)
{
	if (node->val != NULL) {
		struct address_region * region = (struct address_region *)node->val;
		printf("[%lu, %lu]", region->addr_start, region->addr_end);
	}
}

// value compare
int address_comp(void * a, void * b)
{
	printf("begin address_comp()\n");
	if (a != NULL && b != NULL) {
		uint64_t a_val = *((uint64_t *)a);
	    uint64_t b_val = *((uint64_t *)b);
	    printf("compare %lu vs. %lu\n", a_val, b_val);
	    if (a_val > b_val) {
	        return -1;
	    } else if (a_val == b_val) {
	        return 0;
	    }
	    return 1;
	}
	printf("finish address_comp()\n");
    return 0;
}

// value clean
void clean_msb(struct ListNode * node)
{
	struct mem_sampling_backed * msb = (struct mem_sampling_backed *)node->val;
	if (msb != NULL) {
		free(msb->buffer);
		free(msb);
	}
}

void clean_address(struct ListNode * node)
{
	free(node);
}

void clean_region(struct ListNode * node)
{
	free(node);
}
/* --------------------------------------------------------------------------*/


// ----------------------------------------------------------------------------
// Memory operation parser
// ----------------------------------------------------------------------------


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

/* --------------------------------------------------------------------------*/


// ----------------------------------------------------------------------------
// Sample parser
// ----------------------------------------------------------------------------

int sample_parser_init()
{
	stat = (struct mem_sampling_stat *)malloc(sizeof(struct mem_sampling_stat));
	if (stat == NULL)
		return -1;
	/* init stat struct */
	stat->consumed = 0UL;
	stat->na_miss_count = 0UL;
	stat->cache1_count = 0UL;
	stat->cache2_count = 0UL;
	stat->cache3_count = 0UL;
	stat->lfb_count = 0UL;
	stat->memory_count = 0UL;
	stat->remote_memory_count = 0UL;
	stat->remote_cache_count = 0UL;
	stat->total_count = 0UL;
	return 0;
}

/**
 * The sample is composed by 2 part, 1 page metadata + 2^n length ring buffer
 * First we need to know the size of this sample by parsing the field in the 
 * metadata field, then we need to parse the data region
 * @param metadata [description]
 */
int read_sample_data(struct perf_event_mmap_page *metadata, void * buff, size_t * sample_size)
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
	printf("sample_size = %zu\n", *sample_size);

	// parse the data region
	buff = malloc(*sample_size);
	if (buff == NULL) {
		return -1;
	}
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
		memcpy((char*)buff + (metadata->data_size - tail), data_start_addr, head);
	}
	metadata->data_tail = head;
	return 0;
}


void clean_up_sample_parser(struct ListNode * root_msb)
{
	// struct mem_sampling_backed * curr = NULL;
	// while (msb != NULL) {
	// 	curr = msb;
	// 	msb = msb->next;
	// 	free(curr->buffer);
	// 	free(curr);
	// }

	/* clean all linked list */
	list_destroy(&root_region, clean_region);
	list_destroy(&root_address, clean_address);
	list_destroy(&root_msb, clean_msb);
	/* clean memory_sampling_stat struct */
	free(stat);
}

int handler_read_ring_buffer(struct perf_event_mmap_page *metadata_page, struct mem_sampling_backed *new_msb)
{
	// wrap data_head
	if (metadata_page->data_head > metadata_page->data_size) {
		metadata_page->data_head = (metadata_page->data_head % metadata_page->data_size);
	}
	uint64_t head = metadata_page->data_head;
	rmb();
	uint64_t tail = metadata_page->data_tail;
	size_t sample_size;
	if (head > tail) {
		sample_size =  head - tail;
	} else {
		sample_size = (metadata_page->data_size - tail) + head;
	}
	new_msb->sz = sample_size;
	new_msb->buffer = malloc(new_msb->sz);
	if (new_msb->buffer == NULL) {
		fprintf(stderr, "could not malloc new_msb buffer\n");
		return -1;
	}
	uint8_t* start_addr = (uint8_t *)metadata_page;


	/* 
		Here we assume Linux kernel version >= 4.1.0, data_offset is available on if kernel version >= 4.1.0. Otherwise, this will raise an compilation error

		For kernel version less than 4.1.0, use the following code 
		start_addr += (size_t)sysconf(_SC_PAGESIZE);
	*/
	start_addr += metadata_page->data_offset;

	//void* start_address = (char*)metadata_page+measure->page_size+tail;
	if (head > tail) {
		memcpy(new_msb->buffer, start_addr+tail, new_msb->sz);
	} else {
		memcpy(new_msb->buffer, start_addr+tail, (metadata_page->data_size - tail));
		memcpy((char*)new_msb->buffer + (metadata_page->data_size - tail), start_addr, head);
	}

	metadata_page->data_tail = head;
	return 0;
}

int collect_sampling_stat(int fd, struct ListNode * root_msb)
{
	struct ListNode * tmp_header = root_msb;
	uint64_t sample_id = 0UL, sample_count = 0UL;
	while (tmp_header != NULL) {
		if (tmp_header->val == NULL) {
			tmp_header = tmp_header->next;
			continue;
		}
		struct mem_sampling_backed * msb = (struct mem_sampling_backed *)tmp_header->val;
		struct perf_event_header *header;
		if (msb != NULL) {
			header  = (struct perf_event_header *)(msb->buffer);
			size_t ehdr_offset = 0;
			if (msb->fd == fd) {
				sample_count = 0UL; // reset the counter
				while (ehdr_offset < msb->sz) {
					if (header->size == 0) {
						fprintf(stderr, "Error: invalid header size = 0\n");
						goto error;
					}
					if (header -> type == PERF_RECORD_SAMPLE) {
						struct sample_t *sample = (struct sample_t *)((char *)(header) + 8);
						// append this address to the linked list
						list_put(&root_address, (void *)&(sample->addr), sizeof(uint64_t));
						if (is_served_by_local_NA_miss(sample->data_src)) {
							stat->na_miss_count++;
						}
						if (is_served_by_local_cache1(sample->data_src)) {
							stat->cache1_count++;
						}
						if (is_served_by_local_cache2(sample->data_src)) {
							stat->cache2_count++;
						}
						if (is_served_by_local_cache3(sample->data_src)) {
							stat->cache3_count++;
						}
						if (is_served_by_local_lfb(sample->data_src)) {
							stat->lfb_count++;
						}
						if (is_served_by_local_memory(sample->data_src)) {
							stat->memory_count++;
						}
						if (is_served_by_remote_memory(sample->data_src)) {
							stat->remote_memory_count++;
						}
						if (is_served_by_remote_cache_or_local_memory(sample->data_src)) {
							stat->remote_cache_count++;
						}
						stat->total_count++;
						sample_count += 1UL;
					}
					ehdr_offset += header->size;
					header = (struct perf_event_header *)((char*)msb->buffer+ehdr_offset);
				}
			} else { 
				goto error;
			}
			// printf("[sample %lu] has %lu samples\n", sample_id, sample_count);
			sample_id += 1UL;
		} else {
			goto error;
		}
		tmp_header = tmp_header->next;
	}
	return 0;
error:
	return -1;
}

/* construct the page region covered by all samples */
void build_page_region()
{
    // sort all address
    list_sort(&root_address, sizeof(uint64_t), address_comp);
    dump_addresses();
    struct ListNode * curr_address = root_address;
    uint64_t prev_addr = 0UL;
    uint64_t low = 0UL, high = 0UL;
    while (curr_address) {
    	uint64_t addr_val = *((uint64_t *)curr_address->val);
        if (prev_addr == 0UL) {
            low = addr_val;
            high = addr_val;
        } else if (prev_addr+1UL == addr_val) {
            high = addr_val;
        } else if (prev_addr == addr_val) {
            curr_address = curr_address->next;
            continue;
        } else {
            struct address_region * new_region = (struct address_region *) malloc(sizeof(struct address_region));
            new_region->addr_start = low;
            new_region->addr_end = high;
            list_put(&root_region, (void *)new_region, sizeof(struct address_region));
            low = addr_val;
            high = addr_val;
        }
        prev_addr = addr_val;
        curr_address = curr_address->next;
    }
    // append the last memory region
    struct address_region * new_region = (struct address_region *) malloc(sizeof(struct address_region));
    new_region->addr_start = low;
    new_region->addr_end = high;
    list_put(&root_region, (void *)new_region, sizeof(struct address_region));
    printf("Total %zu regions found\n", list_length(root_region));
}

/* --------------------------------------------------------------------------*/



// ----------------------------------------------------------------------------
// Print
// ----------------------------------------------------------------------------
void display_sample_data(struct sample_t * sample)
{
    printf("pc=%" PRIx64 ", @=%" PRIx64 ", src level=%s, latency=%" PRIu64 "\n", sample->ip, sample->addr, get_data_src_level(sample->data_src), sample->weight);
}

void dump_addresses()
{
	list_dump(root_address, dump_single_address);
}

void dump_memory_region()
{
	build_page_region();
	printf("There are in total %zu memory regions\n", list_length(root_region));
	list_dump(root_region, dump_single_region);
}

int display_single_msb(int fd, struct ListNode * msb_node)
{
	if (msb_node == NULL)
		goto error;
	if (msb_node->val != NULL) {
		struct mem_sampling_backed * msb = (struct mem_sampling_backed *)msb_node->val;
		struct perf_event_header *header;
		header  = (struct perf_event_header *)(msb->buffer);
		size_t ehdr_offset = 0;
		if (msb->fd == fd) {
			while (ehdr_offset < msb->sz) {
				if (header->size == 0) {
					fprintf(stderr, "Error: invalid header size = 0\n");
					goto error;
				}
				if (header -> type == PERF_RECORD_SAMPLE) {
					struct sample_t *sample = (struct sample_t *)((char *)(header) + 8);
					display_sample_data(sample);
				}
				ehdr_offset += header->size;
				header = (struct perf_event_header *)((char*)msb->buffer+ehdr_offset);
			}
		} else {
			fprintf(stderr, "fild descriptor does not match\n");
			goto error;
		}
	} else {
		fprintf(stderr, "msb is empty\n");
		goto error;
	}
	printf("\n");
	return 0;
error:
	return -1;
}

void dump_all_samples(int fd, struct ListNode * root_msb)
{
	list_dump(root_msb, dump_single_msb);
	// if (!msb) {
	// 	printf("msb is empty\n");
	// 	return;
	// }
	// int ret;
	// uint64_t sample_id = 0UL, counter = 0UL;
	// struct mem_sampling_backed * curr_msb = msb;
	// while (curr_msb != NULL) {
	// 	if (curr_msb->fd == fd) {
	// 		// drain the sampling buffer
	// 		// we pass fd again because we make dump_single_msb compatable if 
	// 		// the programmer wanna use it elsewhere and it does not check
	// 		// msb->fd == fd before calling this function
	// 		ret = dump_single_msb(fd, &counter, curr_msb, 00);
	// 		if (ret == 0) {
	// 			// printf("sample: %lu, counter: %lu\n", sample_id, counter);
	// 			sample_id += 1UL;
	// 			counter = 0UL;
	// 		}
	// 	}
	// 	curr_msb = curr_msb->next;
	// }
}

void dump_sample_statistics()
{
	printf("%-8lu samples\n", stat->total_count);
	printf("%-8lu %-30s %0.3f%%\n", stat->cache1_count, "local cache 1", (100.0 * stat->cache1_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->cache2_count, "local cache 2", (100.0 * stat->cache2_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->cache3_count, "local cache 3", (100.0 * stat->cache3_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->lfb_count, "local cache LFB", (100.0 * stat->lfb_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->memory_count, "local memory", (100.0 * stat->memory_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->remote_cache_count, "remote cache or local memory", (100.0 * stat->remote_cache_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->remote_memory_count, "remote memory", (100.0 * stat->remote_memory_count / stat->total_count));
	printf("%-8lu %-30s %0.3f%%\n", stat->na_miss_count, "unknown l3 miss", (100.0 * stat->na_miss_count / stat->total_count));
}
/* --------------------------------------------------------------------------*/