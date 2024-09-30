#ifndef LAWD_WEBD_H
#define LAWD_WEBD_H

#include "lawd/error.h"
#include "lawd/http.h"
#include <stdint.h>

/** Simple Web Server */
struct law_webd;

/**
 * Request Handler
 */
typedef int (*law_wd_call_t)(
        struct law_webd *webd,
        struct law_ht_sreq *request,
        struct law_ht_reqhead *head,
        struct law_data *data);

/** HTTP Server Configuration */
struct law_wd_cfg {
        time_t read_head_timeout;               /** Head Timeout */
        FILE *access;                           /** Access Log */
        law_wd_call_t handler;                  /** Request Handler */
        struct law_data *data;                  /** User Data */
};

/** Get the webd's access log. */
FILE *law_wd_access(struct law_webd *webd);

/** Get the webd's worker. */
struct law_worker *law_wd_worker(struct law_webd *webd);

/** 
 * Called at the end of a response. 
 */
sel_err_t law_wd_done(
        struct law_webd *server,
        struct law_ht_sreq *request);

/**
 * Accept the request.
 */
sel_err_t law_wd_accept(
        struct law_server *server,
        struct law_ht_sreq *request,
        struct law_data *data);

#endif