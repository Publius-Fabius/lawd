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
 * ! thread_id [datetime] error action
 * func() : file : line
 * details
 */
sel_err_t law_log_err_ex(
        FILE *stream,
        const char *error,
        const char *action,
        const char *details,
        const char *file,
        const char *func,
        const int line);

/**
 * ! thread_id [datetime] error action 
 * func() : file : line
 * details
 */
sel_err_t law_log_err(
        FILE *stream,
        sel_err_t error,
        const char *action,
        const char *file,
        const char *func,
        const int line);

/** 
 * ! thread_id [datetime] error action 
 * func() : file : line
 * details
 */
#define LAW_LOG_ERR(STREAM, ERROR, ACTION) \
        law_log_err(            \
                STREAM,         \
                ERROR,          \
                ACTION,         \
                __FILE__,       \
                __func__,       \
                __LINE__)

/** 
 * ! thread_id ip-address user [datetime] error action 
 * func() : file : line
 * Connection Reset
 */
sel_err_t law_log_net_err_ex(
        FILE *stream,
        const char *error,
        const char *ip_address,
        const char *user,
        const char *action,
        const char *details,
        const char *file,
        const char *func,
        const int line);

/** 
 * ! thread_id ip-address [datetime] error action 
 * func() : file : line
 * Connection Reset
 */
sel_err_t law_log_net_err(
        FILE *stream,
        const sel_err_t error,
        const int socket,
        const char *user,
        const char *action,
        const char *file,
        const char *func,
        const int line);

/** 
 * ! thread_id ip-address [datetime] error action 
 * func() : file : line
 * Connection Reset
 */
#define LAW_LOG_NET_ERR(STREAM, ERROR, SOCKET, USER, ACTION) \
        law_log_net_err(                \
                STREAM,                 \
                ERROR,                  \
                SOCKET,                 \
                USER,                   \
                ACTION,                 \
                __FILE__,               \
                __func__,               \
                __LINE__)

/** 
 * ip_address ident user [datetime] "req_line" status content_len
 */
sel_err_t law_log_access_ex(
        FILE *access,
        const char *ip_address,
        const char *ident,
        const char *user,
        const char *req_line,
        const int status,
        const size_t content_len);

/** 
 * ip_address ident user [datetime] "req_line" status content_len
 */
sel_err_t law_log_access(
        FILE *access,
        const int socket,
        const char *ident,
        const char *user,
        const char *request_line,
        const int status_code,
        const size_t content_length);

#endif 