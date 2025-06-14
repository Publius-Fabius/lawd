#ifndef LAWD_TIMER_H
#define LAWD_TIMER_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "lawd/time.h"
#include "lawd/id.h"

/** Timer With Versioning */
typedef struct law_timer law_timer_t;

/** Create a new timer */
law_timer_t *law_timer_create(size_t initial_capacity);

/** Destroy the timer */
void law_timer_destroy(law_timer_t *timer);

/** 
 * Insert an expiration, an identifier, and a version number.
 * 
 * @returns A return value of 'false' indicates that the timer is out of 
 * memory.
 */
bool law_timer_insert(
        law_timer_t *timer, 
        law_time_t expiration,
        law_id_t identifier,
        law_version_t version);

/** 
 * Peek timer's minimum element. 
 * 
 * @returns A return value of 'false' indicates that the timer is empty.
 */
bool law_timer_peek( 
        law_timer_t *timer,
        law_time_t *expiration,
        law_id_t *identifier,
        law_version_t *version);

/** 
 * Pop timer's minimum element. 
 * 
 * @returns A return value of 'false' indicates that the timer is empty.
 */
bool law_timer_pop( 
        law_timer_t *timer,
        law_time_t *expiration_date,
        law_id_t *identifier,
        law_version_t *version);

/** 
 * Get the timer's current size.
 */
size_t law_timer_size(law_timer_t *timer);

#endif