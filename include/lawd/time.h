#ifndef LAWD_TIME_H
#define LAWD_TIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lawd/id.h"

#define LAW_TIME_DT_BUF_LEN 24

/** Time Type */
typedef int64_t law_time_t;

/** Datetime Buffer */
struct law_time_dt_buf {
        char bytes[LAW_TIME_DT_BUF_LEN];
};

/** 
 * Get the current time, in milliseconds, since the last epoch. 
 */
law_time_t law_time_millis();

/** 
 * Get a string representing the current datetime in UTC. 
 */
char *law_time_datetime(struct law_time_dt_buf *buffer);

/** 
 * Wait millis milliseconds. 
 */
int law_time_sleep(law_time_t millis);

/** 
 * Timer With Versioning 
 */
typedef struct law_timer law_timer_t;

/** 
 * Create a new timer
 */
law_timer_t *law_timer_create(size_t initial_capacity);

/** 
 * Destroy the timer
 */
void law_timer_destroy(law_timer_t *timer);

/** 
 * Get the timer's current size.
 */
size_t law_timer_size(law_timer_t *timer);

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
        law_vers_t version);

/** 
 * Peek timer's minimum element. 
 * 
 * @returns A return value of 'false' indicates that the timer is empty.
 */
bool law_timer_peek( 
        law_timer_t *timer,
        law_time_t *expiration,
        law_id_t *identifier,
        law_vers_t *version);

/** 
 * Pop timer's minimum element. 
 * 
 * @returns A return value of 'false' indicates that the timer is empty.
 */
bool law_timer_pop( 
        law_timer_t *timer,
        law_time_t *expiration_date,
        law_id_t *identifier,
        law_vers_t *version);

#endif