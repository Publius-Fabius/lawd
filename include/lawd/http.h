#ifndef LAWD_HTTP_H
#define LAWD_HTTP_H

#include "lawd/server.h"
#include "lawd/uri.h"

#include <openssl/ssl.h>

/** HTTP Headers */
struct law_ht_hdrs;

/** HTTP Headers Iterator */
struct law_ht_hdrs_i;

/** HTTP Server Context */
struct law_ht_sctx;

/** HTTP Server-Side Request */
struct law_ht_sreq;

/** HTTP Client-Side Request */
struct law_ht_creq;

/** HTTP Request Head */
struct law_ht_req_head {
        const char *method;                     /** Request Method */
        struct law_uri target;                  /** Target URI */
        const char *version;                    /** HTTP Version */
        struct law_ht_hdrs *headers;            /** Header Fields */
};

/** HTTP Response Head */
struct law_ht_res_head {
        const char *version;                    /** HTTP Version */
        int status;                             /** Status Code */
        struct law_ht_hdrs *headers;            /** Header Fields */
};

/** 
 * HTTP Server Request Handler 
 */
typedef sel_err_t (*law_ht_call_t)(
        struct law_worker *worker,
        struct law_ht_sreq *request,
        struct law_data data);

/** Security Mode */
enum law_ht_security{
        LAW_HT_SSL,                             /** Use OpenSSL Socket IO */
        LAW_HT_UNSECURED                        /** Use System Socket IO */
};

/** HTTP Server Context Configuration */
struct law_ht_sctx_cfg {
        size_t in_length;                       /** Input Buffer Length */
        size_t in_guard;                        /** Input Buffer Guard */
        size_t out_length;                      /** Output Buffer Length */
        size_t out_guard;                       /** Output Buffer Guard */
        size_t heap_length;                     /** Heap Length */
        size_t heap_guard;                      /** Heap Guard */
        law_ht_call_t callback;                 /** Request Callback */
        struct law_data data;                   /** User Data */
        enum law_ht_security security;          /** Security Mode */
        const char *certificate;                /** Certificate File */
        const char *private_key;                /** Private Key File */
};

/** HTTP Client Request Configuration */
struct law_ht_creq_cfg {
        size_t in_length;                       /** Input Buffer Length */
        size_t in_guard;                        /** Input Buffer Guard */
        size_t out_length;                      /** Output Buffer Length */
        size_t out_guard;                       /** Output Buffer Guard */
        size_t heap_length;                     /** Heap Length */
        size_t heap_guard;                      /** Heap Guard */
};

/**
 * Get the header value by name.
 */
const char *law_ht_hdrs_get(
        struct law_ht_hdrs *headers,
        const char *field_name);

/**
 * Get an iterator for the header collection.
 */
struct law_ht_hdrs_i *law_ht_hdrs_elems(struct law_ht_hdrs *headers);

/**
 * Get the next header field <name, value> pair.  A NULL return value indicates
 * the iterator's resources were freed and no cleanup is necessary.  If NULL is
 * returned, the values of (*name) and (*value) are undefined.
 */
struct law_ht_hdrs_i *law_ht_hdrs_i_next(
        struct law_ht_hdrs_i *iterator,
        const char **field_name,
        const char **field_value);

/**
 * Discard an unfinished iteration.
 */
void law_ht_hdrs_i_free(struct law_ht_hdrs_i *iterator);

/** 
 * Get the request's system socket.
 */
int law_ht_sreq_socket(struct law_ht_sreq *request);

/**
 * Get the request's SSL state.
 */
SSL *law_ht_sreq_ssl(struct law_ht_sreq *request);

/** 
 * Get the request's input buffer.
 */
struct pgc_buf *law_ht_sreq_in(struct law_ht_sreq *request);

/** 
 * Get the request's output buffer.
 */
struct pgc_buf *law_ht_sreq_out(struct law_ht_sreq *request);

/** 
 * Get the request's heap.
 */
struct pgc_stk *law_ht_sreq_heap(struct law_ht_sreq *request);

/**
 * Get security mode.
 */
enum law_ht_security law_ht_sreq_security(struct law_ht_sreq *request);

/**
 * Accept an SSL connection.  This function should only be called once even 
 * if it returns LAW_ERR_WNTR or LAW_ERR_WNTW.
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_ssl_accept(struct law_ht_sreq *request);

/**
 * Shutdown an SSL connection and free its resources.  This function should
 * be called multiple times until it returns LAW_ERR_OK in order to fully
 * complete the two way SSL shutdown procedure.
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_ssl_shutdown(struct law_ht_sreq *request);

/**
 * Free any resources acquired by ssl_accept.
 */
void law_ht_sreq_ssl_free(struct law_ht_sreq *request);

/** 
 * Read data into the request's input buffer.
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_read_data(struct law_ht_sreq *request);

/**
 * Read the server-side request's message head.
 * LAW_ERR_WNTR - Wants to read. 
 * LAW_ERR_WNTW - Wants to write. 
 * LAW_ERR_OOB - Buffer is full. 
 * LAW_ERR_SYN - Message syntax. 
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - System error. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK. 
 */
sel_err_t law_ht_sreq_read_head(
        struct law_ht_sreq *request,
        struct law_ht_req_head *head);

/**
 * Set the status line.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_set_status(
        struct law_ht_sreq *request,
        const char *version,
        const int status,
        const char *reason);

/**
 * Add a response header.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_add_header(
        struct law_ht_sreq *request,
        const char *field_name,
        const char *field_value);

/**
 * Begin response body.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_begin_body(struct law_ht_sreq *request);

/**
 * Write data from the output buffer.
 * LAW_ERR_WNTR - Wants to read
 * LAW_ERR_WNTW - Wants to write
 * LAW_ERR_SYS - System error
 * LAW_ERR_SSL - SSL error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_ht_sreq_write_data(struct law_ht_sreq *request);

/**
 * Close the request permanently.
 * LAW_ERR_OK 
 * LAW_ERR_SYS
 */
sel_err_t law_ht_sreq_close(struct law_ht_sreq *request);

/** 
 * Get status code description. 
 */
const char *law_ht_status_str(const int status_code);

/** Sane configuration. */
struct law_ht_sctx_cfg *law_ht_sctx_cfg_sanity();

/** 
 * Create a new HTTP context. 
 */
struct law_ht_sctx *law_ht_sctx_create(struct law_ht_sctx_cfg *conf);

/** 
 * Destroy HTTP context. 
 */
void law_ht_sctx_destroy(struct law_ht_sctx *http);

/**
 * Initialize the HTTP server context.
 * LAW_ERR_SSL
 * LAW_ERR_OK
 */
sel_err_t law_ht_sctx_init(struct law_ht_sctx *context);

/**
 * Entry function for HTTP server functionality.
 */
sel_err_t law_ht_accept(
        struct law_worker *worker,
        int socket,
        struct law_data data);

/**
 * Create a new client-side request.
 */
struct law_ht_creq *law_ht_creq_create(struct law_ht_creq_cfg *config);

/**
 * Destroy the client-side request.
 */
void law_ht_creq_destroy(struct law_ht_creq *request);

/** 
 * Get the request's system socket.
 */
int law_ht_creq_socket(struct law_ht_creq *request);

/**
 * Get the request's SSL state.
 */
SSL *law_ht_creq_SSL(struct law_ht_creq *request);

/**
 * Get the client-side request's output buffer
 */
struct pgc_buf *law_ht_creq_out(struct law_ht_creq *request);

/**
 * Get the client-side request's input buffer.
 */
struct pgc_buf *law_ht_creq_in(struct law_ht_creq *request);

/**
 * Get the client-side request's heap.
 */
struct pgc_stk *law_ht_creq_heap(struct law_ht_creq *request);

/**
 * Establish a network connection with an HTTP service.
 */
sel_err_t law_ht_creq_connect(
        struct law_ht_creq *request, 
        const char *host,
        const char *service);

/**
 * Write the request-line
 */
sel_err_t law_ht_creq_request_line(
        struct law_ht_creq *request,
        const char *method,
        const char *target,
        const char *version);

/**
 * Write a header.
 */
sel_err_t law_ht_creq_header(
        struct law_ht_creq *request,
        const char *field_name,
        const char *field_value);

/**
 * Begin writing the request body.
 */
sel_err_t law_ht_creq_body(struct law_ht_creq *request);

/**
 * Write data from the output buffer.
 */
sel_err_t law_ht_creq_write_data(struct law_ht_creq *request);

/**
 * Send the request.
 */
sel_err_t law_ht_creq_send(struct law_ht_creq *request);

/**
 * Read bytes into the input buffer.
 */
ssize_t law_ht_creq_read_data(struct law_ht_creq *request);

/**
 * Read the client-side request's response head.
 */
sel_err_t law_ht_creq_read_head(
        struct law_ht_creq *request,
        struct law_ht_res_head *head);

/**
 * Done reading response.
 */
sel_err_t law_ht_creq_done(struct law_ht_creq *request);

/* PARSER SECTION */

/*
 *                                 RFC7230 
 *    Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
 * 
 * tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
 *       / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
 *       / DIGIT / ALPHA
 * 
 * token = 1*tchar
 * 
 * method = token
 * 
 * origin_form = absolute_path [ "?" query ]
 * absolute_form = absolute_URI
 * authority_form = authority
 * asterisk_form = "*"
 * 
 * request_target = origin_form
 *                | absolute_form
 *                | authority_form
 *                | asterisk_form
 * 
 * HTTP_name = "HTTP" ; HTTP
 * HTTP_version = HTTP_name "/" DIGIT "." DIGIT
 *
 * request_line = method SP request_target SP HTTP_version CRLF
 * 
 * field_name = token
 * 
 * VCHAR = "any visible USASCII character"
 * 
 * SP = ' '
 * 
 * HTAB = '\t' 
 * 
 * OWS = *( SP | HTAB )
 * 
 * field_vchar = VCHAR
 * 
 * field_content  = field_vchar [ 1*( SP | HTAB ) field_vchar ]
 * 
 * field_value = *( field_content )
 * 
 * header_field = field_name ":" OWS field_value OWS
 * 
 * status-code = 3DIGIT 
 * 
 * reason-phrase = *( HTAB | SP | VCHAR )
 * 
 * status-line = HTTP-version SP status-code SP reason-phrase CRLF
 *
 * start-line = status-line | request-line
 * 
 * message_body = *OCTET
 * 
 * HTTP_message = start_line
 *              *( header_field CRLF )
 *              CRLF
 *              [ message_body ]
 */

/** HTTP Message Tags */
enum law_ht_tag {
        LAW_HT_STATUS,                  /** Status */
        LAW_HT_METHOD,                  /** Method */
        LAW_HT_VERSION,                 /** Version */
        LAW_HT_FIELD,                   /** Header Field */
        LAW_HT_FIELD_NAME,              /** Header Field Name */
        LAW_HT_FIELD_VALUE,             /** Header Field Value */
        LAW_HT_ORIGIN_FORM,             /** Origin URI Form */
        LAW_HT_ABSOLUTE_FORM,           /** Absolute URI Form */
        LAW_HT_AUTHORITY_FORM           /** Authority URI Form */
};

sel_err_t law_ht_cap_status(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_method(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_version(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_field_name(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_field_value(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_field(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_origin_form(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_absolute_form(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_ht_cap_authority_form(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

#endif