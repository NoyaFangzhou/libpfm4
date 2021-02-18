#include "kernel_util.h"

void move_sanity_check(void * list, void ** move_list);


void move_sanity_check(void * list, void ** move_list)
{
	// int i;
	// unsigned long val;
	// for (i = 0; i < (int)vector_length(list); i++) {
	// 	vector_get_by_idx(list, i, &val);
	// 	assert(val == (unsigned long)move_list[i]);
	// }
}

long kernel_move_pages(pid_t pid, unsigned long count, void * pages, const int target_node, void * failed)
{
	if (count == 0)
		return -1;

	int * nodes, * status;
	unsigned long val = 0UL, i;
	long ret, failed_page_cnt = 0L;
	void ** move_list;

	move_list = (void **)calloc(count, sizeof(void *));

	if (!move_list)
		err(1, "move_list allocate failed\n");
	nodes = (int *)malloc(sizeof(int *) * count);
	status = (int *)malloc(sizeof(int *) * count);
	if (!nodes)
		err(1, "nodes allocate failed\n");
	if (!status)
		err(1, "status allocate failed\n");

	/*
		Do the data format convert
		move_pages() API requires the page list to be a void ** type. It also requires a int * to store status for each page and a int * to store the destination node for each page.
	*/
	for (i = 0; i < count; i++) {
		nodes[i] = target_node;
		// vector_get_by_idx(pages, i, &val);
		// The address is 4-byte aligned here
		move_list[i] = (void *)val; 
		status[i] = -123;
	}

	// check whether the transfer is correct
	move_sanity_check(pages, move_list);

	// move hot page in node 1 and replace cold page in node 0
	ret = move_pages(pid, count, move_list, nodes, status, 0);
		

	/*
		Some of the pages cannot be moved.
		We store these pages in the list and when do the post_policy_update
		pages in this list will be ignored.
	 */

	/* if return 0, all pages are moved successfully */
	if (!ret) {
		return 0;
	}

	/* if return negvale, error, no page moved */
	if (ret < 0) {
		return -1;
	}

	/* return the number of pages nomigrated */	
	for (i = 0; i < count; i++) {
		if (status[i] < 0) {
			failed_page_cnt += 1L;
			// vector_put(failed, val);
		}
	}

	assert(ret == failed_page_cnt);

	/* be clean */
	free(move_list);
	free(nodes);
	free(status);
	return failed_page_cnt;
}

long kernel_move_single_page(pid_t pid, unsigned long va, const int target_node)
{
	int node, status;
	long ret;
	void **move_list;

	move_list = (void **)calloc(1, sizeof(void *));

	if (!move_list)
		err(1, "move_list allocate failed\n");

	/*
		Do the data format convert
		move_pages() API requires the page list to be a void ** type. It also requires a int * to store status for each page and a int * to store the destination node for each page.
	*/
	node = target_node;
	move_list[0] = (void *)va;
	status = -123;

	// move this single page to the destination
	ret = move_pages(pid, 1UL, move_list, &node, &status, 0);
		

	/*
		Some of the pages cannot be moved.
		We store these pages in the list and when do the post_policy_update
		pages in this list will be ignored.
	 */

	/* if return 0, all pages are moved successfully */
	if (!ret) {
		return 0;
	}

	/* if return negvale, error, no page moved */
	if (ret < 0) {
		fprintf(stderr, "Error! Move page %lx error\n", va);
		return -1;
	}

	/* return the number of pages nomigrated */
	long failed_page_cnt = 0L;	
	if (status < 0) {
		failed_page_cnt += 1L;
	}
	fprintf(stderr, "Failed to move page %lx with status %d\n", va, status);

	assert(ret == failed_page_cnt);

	/* be clean */
	free(move_list);
	return failed_page_cnt;
}


int kernel_query_node(pid_t pid, unsigned long va)
{
	int status;
	// long ret;
	void **move_list;

	move_list = (void **)calloc(1, sizeof(void *));

	if (!move_list)
		err(1, "move_list allocate failed\n");

	/*
		Do the data format convert
		move_pages() API requires the page list to be a void ** type. It also requires a int * to store status for each page and a int * to store the destination node for each page.
	*/
	move_list[0] = (void *)va;
	status = -123;

	// query the node id this page resides
	// ret = move_pages(pid, 1, move_list, NULL, &status, 0);
	move_pages(pid, 1, move_list, NULL, &status, 0);
	/* be clean */
	free(move_list);
	return status;
}

/* send a signal to the pid based on a given type */
int kernel_send_signal(pid_t pid, int type)
{
	if (type == SIGNAL_PAUSE) {
		return kill(pid, SIGSTOP);
	} else if (type == SIGNAL_CONTINUE) {
		return kill(pid, SIGCONT);
	}
	return -1;
}


void kernel_print_move_error(int status)
{
	switch (status) {
		case -EFAULT:
		fprintf(stderr, "This is a zero page or the memory area is not mapped by the process.\n");
		break;
		case -ENOENT:
		fprintf(stderr, "The given page(s) is/are not present.\n");
		break;
		case -EACCES:
		fprintf(stderr, "The page(s) is/are mapped by multiple processes.\n");
		break;
		case -EBUSY:
		fprintf(stderr, "The page(s) is/are currently busy and cannot be moved.\n");
		break;
		case -EIO:
		fprintf(stderr, "Unable to write back a page.\n");
		break;
		case -EINVAL:
		fprintf(stderr, "The page(s) is/are dirty and cannot be moved.\n");
		break;
		case -ENOMEM:
		fprintf(stderr, "Unable to allocate memory on target node.\n");
		break;
		default:
		break;
	}
}