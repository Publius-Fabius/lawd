#ifndef LAWD_PQUEUE_H
#define LAWD_PQUEUE_H

#include <stdint.h>

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
struct law_pqueue *law_pq_insert(
        struct law_pqueue *queue,
        void *value,
        const int64_t priority);

/** 
 * Pop minimum value, NULL if empty.
 */
struct law_pqueue *law_pq_pop(struct law_pqueue *queue);

/** 
 * Peek minimum value, NULL if empty.
 */
struct law_pqueue *law_pq_peek(
        struct law_pqueue *queue,
        void **value,
        int64_t *priority);

/**
 * Get queue's current capacity.
 */
size_t law_pq_capacity(struct law_pqueue *queue);

/**
 * Get queue's current size.
 */
size_t law_pq_size(struct law_pqueue *queue);

#endif