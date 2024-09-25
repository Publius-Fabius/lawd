#ifndef LAWD_LOG_H
#define LAWD_LOG_H

#include "lawd/error.h"
#include "lawd/uri.h"

/**
 * CLF := ip-address - - [datetime] "request-line" status-code bytes-sent
 */
sel_err_t law_log_access(
        FILE *stream,
        const int socket,
        const char *method,
        struct law_uri *target,
        const char *version,
        const int status_code,
        const size_t content_length);

/**
 * [datetime] pid package "message"
 */
sel_err_t law_log_error(
        FILE *stream,
        const char *package,
        const char *message);

/**
 * ip-address [datetime] pid package "message"
 */
sel_err_t law_log_error_ip(
        FILE *stream,
        const int socket,
        const char *package,
        const char *message);

#endif 