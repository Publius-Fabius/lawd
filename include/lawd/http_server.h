#ifndef LAWD_HTTP_SERVER_H
#define LAWD_HTTP_SERVER_H

#include "lawd/uri.h"
#include "lawd/http_headers.h"
#include "lawd/http_conn.h"
#include "lawd/server.h"

typedef struct law_hts_reqline {                /** HTTP Request Line */
        const char *method;                     /** Request Method */
        law_uri_t target;                       /** Target URI */
        const char *version;                    /** HTTP Version */
} law_hts_reqline_t;

/** HTTP Server Request */
typedef struct law_hts_req law_hts_req_t;

/** HTTP Server Request Handler */
typedef sel_err_t (*law_hts_on_accept_t)(
        law_worker_t *worker,
        law_hts_req_t *request,
        law_hts_reqline_t *reqline,
        law_htheaders_t *headers,
        law_data_t data);

/** HTTP Server Error Handler */
typedef sel_err_t (*law_hts_on_error_t)(
        law_worker_t *worker,
        law_hts_req_t *request,
        sel_err_t error,
        law_data_t data);

typedef struct law_htserver_cfg {               /** HTTP Server Config */
        size_t in;                              /** Input Buffer Length */
        size_t out;                             /** Output Buffer Length */
        size_t stack;                           /** Parser Stack Length */
        size_t heap;                            /** Parser Heap Length */
        law_hts_on_accept_t on_accept;          /** Accept Callback */
        law_hts_on_error_t on_error;            /** Reject Callback */
        law_data_t data;                        /** User Data */
        int security;                           /** Security Mode */
        const char *cert;                       /** Certificate File */
        const char *pkey;                       /** Private Key File */
        time_t ssl_shutdown_timeout;            /** SSL Shutdown Timeout */
        time_t reqline_timeout;                 /** Request Line Timeout */
        time_t headers_timeout;                 /** Headers Timeout */
} law_htserver_cfg_t;

/** HTTP Server */
typedef struct law_htserver law_htserver_t;

/** HTTP Server Request */
typedef struct law_hts_req law_hts_req_t;

/** Get the request's connection. */
law_htconn_t *law_hts_get_conn(law_hts_req_t *request);

/** Get the request's parser stack. */
struct pgc_stk *law_hts_get_stack(law_hts_req_t *request);

/** Get the request's parser heap. */
struct pgc_stk *law_hts_get_heap(law_hts_req_t *request);

/** Get the server's security mode. */
int law_hts_get_security(law_htserver_t *server);

/** 
 * Read the request line.
 * 
 * LAW_ERR_WANTR 
 * LAW_ERR_WANTW 
 * LAW_ERR_OOB
 * LAW_ERR_SYN
 * LAW_ERR_EOF 
 * LAW_ERR_SYS
 * LAW_ERR_SSL
 * LAW_ERR_OK
 */
sel_err_t law_hts_read_reqline(
        law_hts_req_t *request,
        law_hts_reqline_t *reqline,
        const size_t base);

/** 
 * Read the reqline.
 * 
 * LAW_ERR_TIMEOUT
 * LAW_ERR_OOB
 * LAW_ERR_SYN
 * LAW_ERR_EOF 
 * LAW_ERR_SYS
 * LAW_ERR_SSL
 * LAW_ERR_SYN
 * LAW_ERR_OK
 */
sel_err_t law_hts_read_reqline_sync(
        law_worker_t *worker, 
        law_time_t timeout,
        law_hts_req_t *request,
        law_hts_reqline_t *reqline,
        const size_t base);

/** 
 * Read the headers.
 * 
 * LAW_ERR_WANTR 
 * LAW_ERR_WANTW 
 * LAW_ERR_OOB
 * LAW_ERR_SYN
 * LAW_ERR_EOF 
 * LAW_ERR_SYS
 * LAW_ERR_SSL
 * LAW_ERR_SYN
 * LAW_ERR_OK
 */
sel_err_t law_hts_read_headers(
        law_hts_req_t *request,
        law_htheaders_t *headers,
        const size_t base);

/** 
 * Read the headers.
 * 
 * LAW_ERR_TIMEOUT
 * LAW_ERR_OOB
 * LAW_ERR_SYN
 * LAW_ERR_EOF 
 * LAW_ERR_SYS
 * LAW_ERR_SSL
 * LAW_ERR_SYN
 * LAW_ERR_OK
 */
sel_err_t law_hts_read_headers_sync(
        law_worker_t *worker, 
        law_time_t timeout,
        law_hts_req_t *request,
        law_htheaders_t *headers,
        const size_t base);

/** Get status code description. */
const char *law_hts_status_str(const int status_code);

/**
 * Set the status line.
 * 
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_set_status(
        law_hts_req_t *request,
        const char *version,
        const int status,
        const char *reason);

/**
 * Add a response header.
 * 
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_add_header(
        law_hts_req_t *request,
        const char *field_name,
        const char *field_value);

/**
 * Begin response body.
 * 
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_begin_body(law_hts_req_t *request);

/** Sane configuration. */
law_htserver_cfg_t law_hts_sanity();

/** Create a new HTTP context. */
law_htserver_t *law_htserver_create(
        law_htserver_cfg_t *htserver_cfg,
        law_server_cfg_t *server_cfg);

/** Destroy HTTP server. */
void law_htserver_destroy(law_htserver_t *server);

/**
 * Open the HTTP server.
 * 
 * LAW_ERR_SSL
 * LAW_ERR_OK
 */
sel_err_t law_htserver_open(law_htserver_t *server);

/**
 * Close the HTTP server.
 */
sel_err_t law_htserver_close(law_htserver_t *server);

/** Entry function for server. */
sel_err_t law_hts_entry(
        law_worker_t *worker,
        int socket,
        law_data_t data);

#endif