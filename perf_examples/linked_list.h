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
#include <stdint.h>
#include <err.h>

struct ListNode {
    void * val;
    struct ListNode * next;
};

struct ListNode * list_create(void);
struct ListNode * list_node_create(void * val, size_t sz);
void list_put(struct ListNode ** headRef, void * val, size_t sz);
void list_delete_by_idx(struct ListNode ** headRef, uint64_t idx);
uint64_t list_delete_by_val(struct ListNode ** headRef, void * val, int (*comp)(void *, void *));
void list_destroy(struct ListNode ** headRef, void (*clean_up)(struct ListNode *));
void list_sort(struct ListNode ** headRef, size_t sz, int (*comp)(struct ListNode *, struct ListNode *));

void list_iterate(struct ListNode * head, void (*func)(struct ListNode *));
void list_map(struct ListNode * head, void (*func)(struct ListNode *));
void list_filter(struct ListNode ** headRef, int (*cond)(struct ListNode *));

size_t list_length(struct ListNode * head);
void list_dump(struct ListNode * headRef, void(*print)(struct ListNode *) );

#endif /* linked_list_h */
