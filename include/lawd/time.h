#ifndef LAWD_TIME_H
#define LAWD_TIME_H

#include <stdint.h>

#define LAW_TIME_DT_BUF_LEN 24

/** Datetime Buffer */
struct law_time_dt_buf {
        char bytes[LAW_TIME_DT_BUF_LEN];
};

/**
 * Get the current time, in milliseconds, since the last epoch.
 */
int64_t law_time_millis();

/**
 * Get a string representing the current datetime in UTC.
 */
char *law_time_datetime(struct law_time_dt_buf *buffer);

#endif