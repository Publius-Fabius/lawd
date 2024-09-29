#ifndef LAWD_CQUEUE_H
#define LAWD_CQUEUE_H

/** Thread Safe Queue */
struct law_cqueue;

/** 
 * Create a new thread safe queue.
 */
struct law_cqueue *law_cq_create();

/**
 * Destroy the thread safe queue.
 */
void law_cq_destroy(struct law_cqueue *queue);

/**
 * Get the queue's current size.
 */
size_t law_cq_size(struct law_cqueue *queue);

/**
 * Enqueue an object.
 */
struct law_cqueue *law_cq_enq(struct law_cqueue *queue, void *value);

/**
 * Dequeue an object.
 */
struct law_cqueue *law_cq_deq(struct law_cqueue *queue, void **value);

#endif