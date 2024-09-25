#ifndef LAWD_TIME_H
#define LAWD_TIME_H

#include <stdint.h>

#define LAW_TIME_DATETIMESTR_LEN 24

/** Time Value */
typedef int64_t law_time_t;

/**
 * Datetime Buffer
 */
struct law_time_dtb {
        char bytes[LAW_TIME_DATETIMESTR_LEN];
};

/**
 * Get the current time, in milliseconds, since the last epoch.
 * @return The current time.
 */
law_time_t law_time_millis();

/**
 * Get a string representing the current datetime in UTC.
 * @param buffer A buffer large enough to hold the whole string.
 * @return A string representing the current time.
 */
char *law_time_datetime(struct law_time_dtb *buffer);

#endif