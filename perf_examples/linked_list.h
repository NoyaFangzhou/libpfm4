//
//  linked_list.h
//  linked_list
//
//  Created by noya-fangzhou on 2/16/21.
//

#ifndef linked_list_h
#define linked_list_h

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/types.h>
#include <err.h>

struct ListNode {
    void * val;
    struct ListNode * next;
};

struct ListNode * list_create(void);
struct ListNode * list_node_create(void * val, size_t sz);
void list_put(struct ListNode ** headRef, void * val, size_t sz);
void list_destroy(struct ListNode ** headRef, void (*clean_up)(struct ListNode *));
void list_sort(struct ListNode ** headRef, size_t sz, int (*comp)(void *, void *));

size_t list_length(struct ListNode * head);
void list_dump(struct ListNode * headRef, void(*print)(struct ListNode *) );

#endif /* linked_list_h */
