#ifndef LAWD_WEBD_H
#define LAWD_WEBD_H

#include "lawd/error.h"
#include "lawd/http.h"
#include <stdint.h>

/** Simple Web Server */
struct law_webd;

/** Request Head */
struct law_wd_reqhead {
        const char *method;
        struct law_uri *uri;
        const char *version;
        struct law_ht_hdrs *headers;
};

/**
 * Request Handler
 */
typedef int (*law_wd_handler_t)(
        struct law_webd *server,
        struct law_ht_sreq *request,
        struct law_wd_reqhead *head,
        void *state);

/** HTTP Server Configuration */
struct law_webd_cfg {
        time_t read_head_timeout;
        FILE *error_log;
        FILE *access_log;
        law_wd_handler_t handler;
        void *state;
};

/** 
 * Service the request
 */
sel_err_t law_wd_service(
        struct law_srv *server,
        struct law_ht_sreq *request,
        void *state);

/** 
 * Called at the end of a response. 
 */
sel_err_t law_wd_done(
        struct law_webd *server,
        struct law_ht_sreq *request);

/**
 * Flush the output buffer.
 */
sel_err_t law_wd_flush(
        struct law_webd *server,
        struct law_ht_sreq *request,
        law_time_t timeout);

#endif