#ifndef LAWD_PQUEUE_H
#define LAWD_PQUEUE_H

#include <stdint.h>

struct law_pq_pair {
        int64_t priority;
        void *value;
};

/** Priority Queue */
struct law_pqueue;

/**
 * Create a new priority queue.
 */
struct law_pqueue *law_pq_create();

/**
 * Destroy the priority queue.
 */
void law_pq_destroy(struct law_pqueue *queue);

/** 
 * Insert a value.
 */
struct law_pq_pair *law_pq_insert(
        struct law_pqueue *queue,
        const int64_t priority,
        void *value);

/** 
 * Pop minimum, NULL if empty.
 */
struct law_pqueue *law_pq_pop(struct law_pqueue *queue);

/** 
 * Peek minimum, NULL if empty.
 */
struct law_pqueue *law_pq_peek(
        struct law_pqueue *queue,
        int64_t *priority,
        void **value);

/**
 * Get queue's current capacity.
 */
size_t law_pq_capacity(struct law_pqueue *queue);

/**
 * Get queue's current size.
 */
size_t law_pq_size(struct law_pqueue *queue);

#endif