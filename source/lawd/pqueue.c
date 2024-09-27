
#include "lawd/error.h"
#include "lawd/pqueue.h"
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>

/** Queue Item */
struct law_pq_item {
        void *value;
        int64_t pri;
};

/** Priority Queue */
struct law_pqueue {
        struct law_pq_item *items;
        size_t size;
        size_t capacity;
};

size_t law_pq_capacity(struct law_pqueue *queue)
{
        return queue->capacity;
}

size_t law_pq_size(struct law_pqueue *queue)
{
        return queue->size;
}

static void law_pq_swap(struct law_pq_item *a, struct law_pq_item *b) 
{
        struct law_pq_item tmp = *a;
        *a = *b;
        *b = tmp;
}

static void law_pq_heapify_up(struct law_pqueue *q, ssize_t index) 
{
        while(index > 0) {
                ssize_t parent = (index - 1) / 2;
                if(q->items[index].pri < q->items[parent].pri) {
                        law_pq_swap(&q->items[index], &q->items[parent]);
                        index = parent;
                } else {
                        break;
                }
        }
}

static void law_pq_heapify_down(struct law_pqueue *q, size_t index) {
        for(;;) {
                size_t left = 2 * index + 1;
                size_t right = 2 * index + 2;
                size_t min = index;

                if(left < q->size) {
                        if(q->items[left].pri < q->items[min].pri) {
                                min = left;
                                goto SWAP_A;
                        }
                } 
                if(right < q->size) {
                        if(q->items[right].pri < q->items[min].pri) {
                                min = right;
                                goto SWAP_B;
                        } 
                }
                
                break;

                SWAP_A:

                if(right < q->size) {
                        if(q->items[right].pri < q->items[min].pri) {
                                min = right;
                        }
                }

                SWAP_B:

                law_pq_swap(&q->items[index], &q->items[min]);
                index = min;
        }
}

/**
 * Create a new priority queue.
 */
struct law_pqueue *law_pq_create() 
{
        const size_t INIT_CAP = 2;
        struct law_pqueue *queue = malloc(sizeof(struct law_pqueue));
        if(!queue) {
                return NULL;
        }

        queue->items = malloc(INIT_CAP * sizeof(struct law_pq_item));
        if(!queue->items) {
                free(queue);
                return NULL;
        }

        queue->size = 0;
        queue->capacity = INIT_CAP;
        return queue;
}

/**
 * Destroy the priority queue.
 */
void law_pq_destroy(struct law_pqueue *queue)
{
        if(!queue) {
                return;
        }
        free(queue->items);
        free(queue);
}

/** 
 * Insert a value.
 */
struct law_pqueue *law_pq_insert(
        struct law_pqueue *q,
        void *value,
        const int64_t priority)
{
        SEL_ASSERT(q);
        SEL_ASSERT(q->size <= q->capacity);

        if(q->size == q->capacity) {
                size_t new_cap = q->capacity * 2;
                struct law_pq_item *new_items = realloc(
                        q->items, 
                        new_cap * sizeof(struct law_pq_item));
                if(!new_items) {
                        return NULL;
                }
                q->items = new_items;
                q->capacity = new_cap;
        }

        q->items[q->size].value = value;
        q->items[q->size].pri = priority;
        law_pq_heapify_up(q, (ssize_t)q->size);
        q->size += 1;

        return q;
}

/** 
 * Pop minimum value, NULL if empty.
 */
struct law_pqueue *law_pq_pop(struct law_pqueue *q)
{
        SEL_ASSERT(q);

        if(q->size == 0) {
                return NULL;
        }

        q->size -= 1;
        law_pq_swap(&q->items[0], &q->items[q->size]);
        law_pq_heapify_down(q, 0);

        return q;
}

/** 
 * Peek minimum value, NULL if empty.
 */
struct law_pqueue *law_pq_peek(
        struct law_pqueue *queue,
        void **value,
        int64_t *priority)
{
        SEL_ASSERT(queue);

        if(queue->size == 0) {
                return NULL;
        }
        if(value) {
                *value = queue->items[0].value;
        }
        if(priority) {
                *priority = queue->items[0].pri;
        }
        return queue;
}
