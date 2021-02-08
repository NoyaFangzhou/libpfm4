//
//  linked_list.c
//  linked_list
//
//  Created by noya-fangzhou on 2/16/21.
//

#include "linked_list.h"

/* helper function used to sort linked list */
struct ListNode * sorted_merge(struct ListNode * a, struct ListNode * b, int (*comp)(void *, void *));
void front_back_split(struct ListNode * source, struct ListNode ** frontRef, struct ListNode ** backRef);

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
        unsigned long i;
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
    if (head)
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

/* sorts the linked list by changing next pointers (not data) */
void list_sort(struct ListNode ** headRef, size_t sz, int (*comp)(void *, void *))
{
    struct ListNode * head = *headRef;
    struct ListNode * a;
    struct ListNode * b;
  
    /* Base case -- length 0 or 1 */
    if ((head == NULL) || (head->next == NULL)) {
        return;
    }
  
    /* Split head into 'a' and 'b' sublists */
    front_back_split(head, &a, &b);
    // printf("front_back_split()\n");
  
    /* Recursively sort the sublists */
    list_sort(&a, sz, comp);
    // printf("sort sub list a (%zu)\n", list_length(a));
    list_sort(&b, sz, comp);
    // printf("sort sub list b (%zu)\n", list_length(b));
  
    /* answer = merge the two sorted lists together */
    *headRef = sorted_merge(a, b, comp);
    // printf("sorted_merge a, b (%zu)\n", list_length(*headRef));
}
  
/* See https:// www.geeksforgeeks.org/?p=3622 for details of this
function */
// comp
// a < b : return 1;
// a == b: return 0;
// a > b : return -1;
struct ListNode * sorted_merge(struct ListNode * a, struct ListNode * b, int (*comp)(void *, void *))
{
    printf("begin sorted_merge\n");
    struct ListNode * result = NULL;
  
    /* Base cases */
    if (a == NULL) {
        printf("a is NULL, append b to the rest\n");
        return b;
    } else if (b == NULL) {
        printf("b is NULL, append a to the rest\n");
        return a;
    }
    printf("begin compare a->val and b->val \n");
    /* Pick either a or b, and recur */
    if (comp(a->val, b->val) >= 0) {
        result = a;
        printf("a(%zu) <= b(%zu), append a node to result\n", list_length(a), list_length(b));

        result->next = sorted_merge(a->next, b, comp);
    } else {
        result = b;
        printf("a(%zu) > b(%zu), append b node to result\n", list_length(a), list_length(b));
        result->next = sorted_merge(a, b->next, comp);
    }
    printf("finish sorted_merge\n");
    return result;
}
  
/* UTILITY FUNCTIONS */
/* Split the nodes of the given list into front and back halves,
    and return the two lists using the reference parameters.
    If the length is odd, the extra node should go in the front list.
    Uses the fast/slow pointer strategy. */
void front_back_split(struct ListNode * source,
                     struct ListNode ** frontRef, struct ListNode ** backRef)
{
    struct ListNode * fast;
    struct ListNode * slow;
    slow = source;
    fast = source->next;
  
    /* Advance 'fast' two nodes, and advance 'slow' one node */
    while (fast != NULL) {
        fast = fast->next;
        if (fast != NULL) {
            slow = slow->next;
            fast = fast->next;
        }
    }
  
    /* 'slow' is before the midpoint in the list, so split it in two
    at that point. */
    *frontRef = source;
    *backRef = slow->next;
    slow->next = NULL;
}
