#ifndef LAWD_LOG_H
#define LAWD_LOG_H

#include "lawd/error.h"
#include "lawd/uri.h"

/**
 * CLF := ip-address - - [datetime] "request-line" status-code bytes-sent
 */
sel_err_t law_log_access(
        FILE *access,
        const int socket,
        const char *method,
        struct law_uri *target,
        const char *version,
        const int status_code,
        const size_t content_length);

/**
 * [datetime] pid tid package action "message"
 */
sel_err_t law_log_error(
        FILE *stream,
        const char *action,
        const char *message);

/**
 * ip-address [datetime] pid tid action "message"
 */
sel_err_t law_log_error_ip(
        FILE *stream,
        const int socket,
        const char *action,
        const char *message);

#endif 