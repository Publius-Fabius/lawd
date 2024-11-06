
#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include "lawd/time.h"

char *law_time_datetime(struct law_time_dt_buf *buf)
{
        /* 2007-04-05T14:30Z */
        static const char *FORMAT = "%Y-%m-%dT%H:%M:%SZ";
        struct tm tm;
        time_t t;
        time(&t);
        gmtime_r(&t, &tm);
        strftime(buf->bytes, LAW_TIME_DT_BUF_LEN, FORMAT, &tm);
        return buf->bytes;
}

int64_t law_time_millis()
{
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return (int64_t)(ts.tv_sec) * 1000 + (int64_t)(ts.tv_nsec) / 1000000;
}
