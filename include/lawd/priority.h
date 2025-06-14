#ifndef LAWD_PRIORITY_H
#define LAWD_PRIORITY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "lawd/data.h"

/** Priority Queue Comparison Function */
typedef int (*law_pq_cmp_t)(struct law_data a, struct law_data b);

/** Priority Queue */
struct law_pq;

/** Create a new priority queue. */
struct law_pq *law_pq_create(law_pq_cmp_t compare);

/** Destroy the priority queue. */
void law_pq_destroy(struct law_pq *queue);

/** Get the queue's current capacity. */
size_t law_pq_capacity(struct law_pq *queue);

/** Get the queue's current size. */
size_t law_pq_size(struct law_pq *queue);

/** Add a value to the priority queue. */
bool law_pq_insert(
        struct law_pq *queue,
        struct law_data priority,
        const uint32_t version,
        void *value);

/** Pop minimum. */
bool law_pq_pop(
        struct law_pq *queue,
        struct law_data *priority,
        uint32_t *version,
        void **value);

/** Peek minimum. */
bool law_pq_peek(
        struct law_pq *queue,
        struct law_data *priority,
        uint32_t *version,
        void **value);

#endif