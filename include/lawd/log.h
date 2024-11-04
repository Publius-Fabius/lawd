#ifndef LAWD_LOG_H
#define LAWD_LOG_H

#include "lawd/error.h"
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
 * [datetime] pid action "message"
 */
void law_log_error(
        FILE *stream,
        const char *action,
        const char *message);

/**
 * [datetime] [ip-address] pid action "message"
 */
sel_err_t law_log_error_ip(
        FILE *stream,
        const int socket,
        const char *action,
        const char *message);

#endif 