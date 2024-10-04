#ifndef LAWD_WEBD_H
#define LAWD_WEBD_H

#include "lawd/error.h"
#include "lawd/http.h"
#include <stdint.h>

/** Simple Web Server */
struct law_webd;

/**
 * Request Handler: called per request, can be called multiple times per 
 * connection if pipelining is enabled.
 */
typedef sel_err_t (*law_wd_handler_t)(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *request,
        struct law_ht_reqhead *head,
        struct law_data data);

/**
 * Error Code Handler: called when a connection can still support a response
 * with a valid status code.
 */
typedef sel_err_t (*law_wd_onerror_t)(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *request,
        const int status_code,
        struct law_data data);

/** HTTP Server Configuration */
struct law_wd_cfg {
        time_t read_head_timeout;               /** Head Timeout */
        time_t ssl_shutdown_timeout;            /** SSL Shutdown Timeout */
        FILE *access;                           /** Access Log */
        law_wd_handler_t handler;               /** Request Handler */
        law_wd_onerror_t onerror;               /** Error Handler */
        struct law_data data;                   /** User Data */
};

/**
 * Sane configuration.
 */
struct law_wd_cfg *law_wd_cfg_sanity();

/**
 * Create a new webd.
 */
struct law_webd *law_wd_create(struct law_wd_cfg *cfg);

/**
 * Destroy the webd.
 */
void law_wd_destroy(struct law_webd *webd);

/** 
 * Get the webd's access log. 
 */
FILE *law_wd_access(struct law_webd *webd);

/** 
 * Called at the end of a response. 
 */
sel_err_t law_wd_done(
        struct law_webd *webd,
        struct law_ht_sreq *request);

/**
 * Accept the request.
 */
sel_err_t law_wd_accept(
        struct law_worker *worker,
        struct law_ht_sreq *request,
        struct law_data data);

#endif