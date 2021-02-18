//
//  linked_list.c
//  linked_list
//
//  Created by noya-fangzhou on 2/16/21.
//

#include "linked_list.h"

struct ListNode* quickSortRecur(struct ListNode* head,
                            struct ListNode* end,
                            int (*comp)(struct ListNode *, struct ListNode *));
struct ListNode* partition(struct ListNode* head, struct ListNode* end,
                       struct ListNode** newHead,
                       struct ListNode** newEnd, 
                       int (*comp)(struct ListNode *, struct ListNode *));
struct ListNode* getTail(struct ListNode* cur);

struct ListNode * list_create(void)
{
    struct ListNode * head = (struct ListNode *)malloc(sizeof(struct ListNode));
    if (!head)
        err(1, "cannot allocate list head");
    head->next = NULL;
    return head;
}

struct ListNode * list_node_create(void * val, size_t sz)
{
    struct ListNode * node = (struct ListNode *)malloc(sizeof(struct ListNode));
    if (node) {
        node->val = malloc(sz);
        node->next = NULL;
        uint64_t i;
        for (i = 0UL; i < sz; i++) {
            *(char *)(node->val + i) = *(char *)(val + i);
        }
    } else {
        err(1, "cannot allocate list node");
    }
    return node;
}

void list_put(struct ListNode ** headRef, void * val, size_t sz)
{
    if (!*headRef) {
        *headRef = list_node_create(val, sz);
    } else {
//        struct ListNode * tmp_header = *headRef;
//        while (tmp_header->next != NULL) {
//            tmp_header = tmp_header->next;
//        }
        struct ListNode * node = list_node_create(val, sz);
        node->next = *headRef;
        *headRef = node;
    }
}

void list_destroy(struct ListNode ** headRef, void (*clean_up)(struct ListNode *))
{
    struct ListNode * current = *headRef;
    while (*headRef) {
        current = *headRef;
        *headRef = (*headRef)->next;
        clean_up(current);
    }
}

size_t list_length(struct ListNode * head)
{
    if (!head)
        return 0;
    struct ListNode * tmp_header = head;
    uint64_t length = 0UL;
    while (tmp_header) {
        length += 1;
        tmp_header = tmp_header->next;
    }
    return (size_t)length;
}

void list_dump(struct ListNode * head, void(*print)(struct ListNode *) )
{
    if (head == NULL)
        printf("The given linked list is empty\n");
    struct ListNode * tmp_header = head;
    while (tmp_header) {
        print(tmp_header);
        if (tmp_header->next) {
            printf(" -> ");
        }
        tmp_header = tmp_header->next;
    }
    printf("\n");
}

/*
    sorts the linked list by changing next pointers (not data)
    comp()
    a->val < b->val : return 1;
    a->val == b->val: return 0;
    a->val > b->val : return -1;
 */
void list_sort(struct ListNode ** headRef, size_t sz, int (*comp)(struct ListNode *, struct ListNode *))
{
    // The main function for quick sort. This is a wrapper over
    // recursive function quickSortRecur()
    (*headRef) = quickSortRecur(*headRef, getTail(*headRef), comp);
}

// Returns the last node of the list
struct ListNode* getTail(struct ListNode* cur)
{
    while (cur != NULL && cur->next != NULL)
        cur = cur->next;
    return cur;
}
 
// Partitions the list taking the last element as the pivot
struct ListNode* partition(struct ListNode* head, struct ListNode* end,
                       struct ListNode** newHead,
                       struct ListNode** newEnd,
                       int (*comp)(struct ListNode *, struct ListNode *))
{
    struct ListNode* pivot = end;
    struct ListNode *prev = NULL, *cur = head, *tail = pivot;
 
    // During partition, both the head and end of the list
    // might change which is updated in the newHead and
    // newEnd variables
    while (cur != pivot) {
        if (comp(cur, pivot) >= 0) {
            // First node that has a value less than the
            // pivot - becomes the new head
            if ((*newHead) == NULL)
                (*newHead) = cur;
 
            prev = cur;
            cur = cur->next;
        }
        else // If cur node is greater than pivot
        {
            // Move cur node to next of tail, and change
            // tail
            if (prev)
                prev->next = cur->next;
            struct ListNode* tmp = cur->next;
            cur->next = NULL;
            tail->next = cur;
            tail = cur;
            cur = tmp;
        }
    }
 
    // If the pivot data is the smallest element in the
    // current list, pivot becomes the head
    if ((*newHead) == NULL)
        (*newHead) = pivot;
 
    // Update newEnd to the current last node
    (*newEnd) = tail;
 
    // Return the pivot node
    return pivot;
}
 
// here the sorting happens exclusive of the end node
struct ListNode* quickSortRecur(struct ListNode* head,
                            struct ListNode* end,
                            int (*comp)(struct ListNode *, struct ListNode *))
{
    // base condition
    if (!head || head == end)
        return head;
 
    struct ListNode *newHead = NULL, *newEnd = NULL;
 
    // Partition the list, newHead and newEnd will be
    // updated by the partition function
    struct ListNode* pivot = partition(head, end, &newHead, &newEnd, comp);
 
    // If pivot is the smallest element - no need to recur
    // for the left part.
    if (newHead != pivot) {
        // Set the node before the pivot node as NULL
        struct ListNode* tmp = newHead;
        while (tmp->next != pivot)
            tmp = tmp->next;
        tmp->next = NULL;
 
        // Recur for the list before pivot
        newHead = quickSortRecur(newHead, tmp, comp);
 
        // Change next of last node of the left half to
        // pivot
        tmp = getTail(newHead);
        tmp->next = pivot;
    }
 
    // Recur for the list after the pivot element
    pivot->next = quickSortRecur(pivot->next, newEnd, comp);
 
    return newHead;
}