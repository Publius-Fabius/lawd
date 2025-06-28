#ifndef LAWD_LOG_H
#define LAWD_LOG_H

#include "lawd/error.h"
#include "lawd/uri.h"
#include <netinet/in.h>

/** IP Address String */
struct law_log_ip_buf {
        char bytes[INET6_ADDRSTRLEN];
};

/**
 * Get a string representation of the socket's IP address.  Returns NULL on 
 * error.
 */
char *law_log_ntop(
        const int socket, 
        struct law_log_ip_buf *ip_addr);

/** 
 * [datetime] action error_type \r\n
 *     Error Details: details \r\n
 *     Location: file func line \r\n
 */
sel_err_t law_log_err(
        FILE *stream,
        const char *action,
        sel_err_t error,
        const char *details,
        const char *file,
        const char *func,
        const int line);

/** 
 * [datetime] action error_type \r\n
 *     Error Details: error_details \r\n
 *     Location: file func line \r\n
 */
#define LAW_LOG_ERR(STREAM, ACTION, ERROR, DETAILS)     \
        law_log_err(                                    \
                STREAM,                                 \
                ACTION,                                 \
                ERROR,                                  \
                DETAILS,                                \
                __FILE__,                               \
                __func__,                               \
                __LINE__)

/** 
 * [datetime] [ip-address] pid action error_type \r\n
 *     Error Details: error_details \r\n
 *     Location: file func line \r\n
 */
sel_err_t law_log_net_err(
        FILE *stream,
        const int socket,
        const char *action,
        sel_err_t error,
        const char *details,
        const char *file,
        const char *func,
        const int line);

/** 
 * [datetime] [ip-address] action error_type \r\n
 *     Error Details: error_details \r\n
 *     Location: file func line \r\n
 */
#define LAW_LOG_NET_ERR(STREAM, SOCKET, ACTION, ERROR, DETAILS) \
                law_log_net_err(        \
                STREAM,                 \
                SOCKET,                 \
                ACTION,                 \
                ERROR,                  \
                DETAILS,                \
                __FILE__,               \
                __func__,               \
                __LINE__)

/** Log HTTP access in CLF */
sel_err_t law_log_access(
        FILE *access,
        const int socket,
        const char *request_method,
        struct law_uri *target_uri,
        const char *http_version,
        const int status_code,
        const size_t content_length);

#endif 