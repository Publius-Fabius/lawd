#ifndef LAWD_HTTP_SERVER_H
#define LAWD_HTTP_SERVER_H

#include "lawd/uri.h"
#include "lawd/http/headers.h"
#include "lawd/http/conn.h"
#include "lawd/server.h"
#include <openssl/ssl.h>

/** HTTP Server Request */
struct law_hts_req;

/** HTTP Server Request Handler */
typedef sel_err_t (*law_hts_call_t)(
        struct law_worker *worker,
        struct law_hts_req *request,
        struct law_data data);

/** HTTP Server Configuration */
struct law_hts_cfg {
        size_t in_length;                       /** Input Buffer Length */
        size_t in_guard;                        /** Input Buffer Guard */
        size_t out_length;                      /** Output Buffer Length */
        size_t out_guard;                       /** Output Buffer Guard */
        size_t stack_length;                    /** Parser Stack Length */
        size_t stack_guard;                     /** Parser Stack Guard */
        size_t heap_length;                     /** Heap Length */
        size_t heap_guard;                      /** Heap Guard */
        law_hts_call_t callback;                /** Request Callback */
        law_data_t data;                        /** User Data */
        int security;                           /** Security Mode */
        const char *cert;                       /** Certificate File */
        const char *pkey;                       /** Private Key File */
};

/** HTTP Server Context */
struct law_hts_ctx {
        struct law_hts_cfg cfg;
        SSL_CTX *ssl_ctx;
};

/** HTTP Server Request */
struct law_hts_req {
        struct law_htconn conn;
        struct pgc_stk *stack, *heap;
        struct law_hts_ctx *ctx;
};

/** HTTP Server Request Head */
struct law_hts_head {
        const char *method;                     /** Request Method */
        struct law_uri target;                  /** Target URI */
        const char *version;                    /** HTTP Version */
        struct law_hthdrs *headers;             /** Header Fields */
};

/**
 * Accept an SSL connection.  This function should only be called once even 
 * if it returns LAW_ERR_WNTR or LAW_ERR_WNTW.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_ssl_accept(struct law_hts_req *request);

/**
 * Shutdown an SSL connection and free its resources.  This function should
 * be called multiple times until it returns LAW_ERR_OK in order to fully
 * complete the two way SSL shutdown procedure.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_ssl_shutdown(struct law_hts_req *request);

/** Free any resources acquired by ssl_accept. */
sel_err_t law_hts_ssl_free(struct law_hts_req *request);

/**
 * Read the server-side request's message head.
 * 
 * LAW_ERR_WNTR - Wants to read. 
 * LAW_ERR_WNTW - Wants to write. 
 * LAW_ERR_OOB - Buffer is full. 
 * LAW_ERR_SYN - Message syntax. 
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - System error. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK. 
 */
sel_err_t law_hts_read_head(
        struct law_hts_req *request,
        struct law_hts_head *head);

/** Get status code description. */
const char *law_hts_status_str(const int status_code);

/**
 * Set the status line.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_set_status(
        struct law_hts_req *request,
        const char *version,
        const int status,
        const char *reason);

/**
 * Add a response header.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_add_header(
        struct law_hts_req *request,
        const char *field_name,
        const char *field_value);

/**
 * Begin response body.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_hts_begin_body(struct law_hts_req *request);

/** Sane configuration. */
struct law_hts_cfg law_hts_sanity();

/** Create a new HTTP context. */
struct law_hts_ctx *law_hts_create(struct law_hts_cfg *conf);

/** Destroy HTTP context. */
void law_hts_destroy(struct law_hts_ctx *context);

/**
 * Initialize the server's SSL context.
 * LAW_ERR_SSL
 * LAW_ERR_OK
 */
sel_err_t law_hts_init_ssl_ctx(struct law_hts_ctx *context);

/** Entry function for HTTP server functionality. */
sel_err_t law_hts_accept(
        struct law_worker *worker,
        int socket,
        struct law_data data);

#endif