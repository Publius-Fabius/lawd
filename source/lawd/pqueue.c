
#include "lawd/error.h"
#include "lawd/pqueue.h"
#include <stddef.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>

#ifndef LAW_PQ_INIT_CAP
#define LAW_PQ_INIT_CAP 2
#endif

/** Priority Queue */
struct law_pqueue {
        struct law_pq_pair **pairs;
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

static void law_pq_swap(struct law_pq_pair **a, struct law_pq_pair **b) 
{
        struct law_pq_pair *tmp = *a;
        *a = *b;
        *b = tmp;
}

static void law_pq_heapify_up(struct law_pqueue *q, ssize_t index) 
{
        while(index > 0) {
                ssize_t parent = (index - 1) / 2;
                if(q->pairs[index]->priority < q->pairs[parent]->priority) {
                        law_pq_swap(q->pairs + index, q->pairs + parent);
                        index = parent;
                } else {
                        break;
                }
        }
}

static void law_pq_heapify_down(struct law_pqueue *q, size_t index) {
        for(;;) {
                size_t l = 2 * index + 1;
                size_t r = 2 * index + 2;
                size_t min = index;

                if(l < q->size) {
                        if(q->pairs[l]->priority < q->pairs[min]->priority) {
                                min = l;
                                goto SWAP_A;
                        }
                } 
                if(r < q->size) {
                        if(q->pairs[r]->priority < q->pairs[min]->priority) {
                                min = r;
                                goto SWAP_B;
                        } 
                }
                
                break;

                SWAP_A:

                if(r < q->size) {
                        if(q->pairs[r]->priority < q->pairs[min]->priority) {
                                min = r;
                        }
                }

                SWAP_B:

                law_pq_swap(q->pairs + index, q->pairs + min);
                index = min;
        }
}

/**
 * Create a new priority queue.
 */
struct law_pqueue *law_pq_create() 
{
        struct law_pqueue *queue;
        struct law_pq_pair **pairs;
        size_t n;
        queue = malloc(sizeof(struct law_pqueue));
        if(!queue) {
                return NULL;
        }
        pairs = malloc(LAW_PQ_INIT_CAP * sizeof(struct law_pq_pair*));
        if(!pairs) {
                goto FREE_QUEUE;
        }
        for(n = 0; n < LAW_PQ_INIT_CAP; ++n) {
                pairs[n] = calloc(1, sizeof(struct law_pq_pair));
                if(!pairs[n]) {
                        goto FREE_PAIRS;
                }
        }

        queue->pairs = pairs;
        queue->capacity = LAW_PQ_INIT_CAP;
        queue->size = 0;
        return queue;

        FREE_PAIRS:
        for(size_t m = 0; m < n; ++m) {
                free(pairs[m]);
        }
        free(pairs);
        FREE_QUEUE:
        free(queue);
        return NULL;
}

/**
 * Destroy the priority queue.
 */
void law_pq_destroy(struct law_pqueue *queue)
{
        if(!queue) {
                return;
        }
        for(size_t n = 0; n < queue->capacity; ++n) {
                free(queue->pairs[n]);
        }
        free(queue->pairs);
        free(queue);
}

static struct law_pqueue *law_pq_grow(struct law_pqueue *q)
{
        const size_t old_cap = q->capacity;
        const size_t new_cap = old_cap + old_cap;
        struct law_pq_pair **new_pairs;
        new_pairs = realloc(q->pairs, new_cap * sizeof(void*));
        if(!new_pairs) {
                return NULL;
        } else {
                q->pairs = new_pairs;
                q->capacity = new_cap;  
        }
        memset(new_pairs + old_cap, 0, old_cap * sizeof(void*));
        for(size_t n = old_cap; n < new_cap; ++n) {
                new_pairs[n] = calloc(1, sizeof(struct law_pq_pair));
                if(!new_pairs[n]) {
                        return NULL;
                }
        }
        return q;
}

/** 
 * Insert a value.
 */
struct law_pq_pair *law_pq_insert(
        struct law_pqueue *q,
        const int64_t priority,
        void *value)
{
        SEL_ASSERT(q);
        SEL_ASSERT(q->size <= q->capacity);
        if(q->size == q->capacity) {
                if(!law_pq_grow(q)) {
                        return NULL;
                }
        }
        struct law_pq_pair *pair = q->pairs[q->size];
        pair->value = value;
        pair->priority = priority;
        law_pq_heapify_up(q, (ssize_t)q->size++);
        return pair;
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
        law_pq_swap(q->pairs, q->pairs + --q->size);
        law_pq_heapify_down(q, 0);
        return q;
}

/** 
 * Peek minimum value, NULL if empty.
 */
struct law_pqueue *law_pq_peek(
        struct law_pqueue *queue,
        int64_t *priority,
        void **value)
{
        SEL_ASSERT(queue);
        if(queue->size == 0) {
                return NULL;
        }
        if(value) {
                *value = queue->pairs[0]->value;
        }
        if(priority) {
                *priority = queue->pairs[0]->priority;
        }
        return queue;
}
