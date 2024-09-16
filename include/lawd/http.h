#ifndef LAWD_HTTP_H
#define LAWD_HTTP_H

#include "lawd/server.h"
#include "lawd/uri.h"
#include <stdio.h>
#include <sys/socket.h>

/** HTTP Method */
enum law_ht_method {
        LAW_HT_GET,                     /** Get Method */
        LAW_HT_POST,                    /** Post Method */
        LAW_HT_HEAD,                    /** Head Method */
        LAW_HT_PUT,                     /** Put Method */
        LAW_HT_DELETE,                  /** Delete Method */
        LAW_HT_PATCH,                   /** Patch Method */
        LAW_HT_OPTIONS,                 /** Options Method */
        LAW_HT_TRACE,                   /** Trace Method */
        LAW_HT_CONNECT                  /** Connect Method */
};

/** HTTP Version */
enum law_ht_version {
        LAW_HT_1_1,                     /** Version 1.1 */
        LAW_HT_2                        /** Version 2 */
};

/** HTTP Message Tags */
enum law_ht_tag {
        LAW_HT_METHOD,                  /** Method */
        LAW_HT_VERSION,                 /** Version */
        LAW_HT_FIELD,                   /** Header Field */
        LAW_HT_FIELD_NAME,              /** Header Field Name */
        LAW_HT_FIELD_VALUE,             /** Header Field Value */
        LAW_HT_ORIGIN_FORM,             /** Origin URI Form */
        LAW_HT_ABSOLUTE_FORM,           /** Absolute URI Form */
        LAW_HT_AUTHORITY_FORM           /** Authority URI Form */
};

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

/** 
 * HTTP Server Request Handler 
 * @param server The network server
 * @param context The HTTP server context.
 * @return A possible error.
 */
typedef sel_err_t (*law_ht_handler_t)(
        struct law_srv *server,
        struct law_ht_sctx *context,
        struct law_ht_sreq *request);

/** HTTP Server Context Configuration */
struct law_ht_sctx_cfg {
        size_t in_length;                       /** Input Buffer Length */
        size_t in_guard;                        /** Input Buffer Guard */
        size_t out_length;                      /** Output Buffer Length */
        size_t out_guard;                       /** Output Buffer Guard */
        size_t heap_length;                     /** Heap Length */
        size_t heap_guard;                      /** Heap Guard */
        law_ht_handler_t handler;               /** Request Handler */
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
 * @param headers A collection of HTTP message headers.
 * @param name The header field to search for.
 * @return The field's value or NULL.
 */
const char *law_ht_hdrs_get(
        struct law_ht_hdrs *headers,
        const char *name);

/**
 * Get an iterator for the header collection.
 * @param headers A collection of HTTP message headers.
 * @return The header collection iterator.
 */
struct law_ht_hdrs_i *law_ht_hdrs_elems(
        struct law_ht_hdrs *headers);

/**
 * Get the next header field <name, value> pair.
 */
struct law_ht_hdrs_i *law_ht_hdrs_i_next(
        struct law_ht_hdrs_i *iterator,
        const char **name,
        const char **value);

/** 
 * Get the request's input buffer.
 * @param request The server-side request.
 * @return The request's input buffer.
 */
struct pgc_buf *law_ht_sreq_in(
        struct law_ht_sreq *request);

/** 
 * Get the request's output buffer.
 * @param request The server-side request.
 * @return The request's output buffer.
 */
struct pgc_buf *law_ht_sreq_out(
        struct law_ht_sreq *request);

/** 
 * Get the request's heap.
 * @param request The server-side request.
 * @return The request's heap.
 */
struct pgc_stk *law_ht_sreq_heap(
        struct law_ht_sreq *request);

/**
 * Read the server-side request's message head.
 * @return 
 * LAW_ERR_TRY_AGAIN - 
 * LAW_ERR_BUFFER_LIMIT - 
 * LAW_ERR_UNEXPECTED_EOF - 
 * LAW_ERR_SOCKET_IO - 
 * LAW_ERR_MALFORMED_MESSAGE - 
 * LAW_ERR_METHOD_NOT_ALLOWED - 
 * LAW_ERR_UNSUPPORTED_VERSION - 
 * SEL_ERR_OK 
 */
sel_err_t law_ht_sreq_read_head(
        struct law_ht_sreq *request,
        enum law_ht_method *method,
        enum law_ht_version *version,
        struct law_uri *target,
        struct law_ht_hdrs **headers);

/** 
 * Read data into the request's input buffer.
 */
sel_err_t law_ht_sreq_read_data(
        struct law_ht_sreq *request);

/**
 * Write the server-side request's response head.
 * @param request The server-side request.
 */
sel_err_t law_ht_sreq_write_head(
        struct law_ht_sreq *request,
        const int status,
        const size_t header_count,
        const char *headers[][2]);

/**
 * Write data from the output buffer.
 */
ssize_t law_ht_sreq_write_data(
        struct law_ht_sreq *request);

/**
 * Done writing response body.
 */
sel_err_t law_ht_sreq_done(
        struct law_ht_sreq *request);

/* CLIENT REQUEST SECTION */

/**
 * Create a new client-side request.
 */
struct law_ht_creq *law_ht_creq_create(
        struct law_ht_creq_cfg *config);

/**
 * Destroy the client-side request.
 */
void law_ht_creq_destroy(
        struct law_ht_creq *request);

/**
 * Get the client-side request's output buffer
 */
struct pgc_buf *law_ht_creq_out(
        struct law_ht_creq *request);

/**
 * Get the client-side request's input buffer.
 */
struct pgc_buf *law_http_creq_in(
        struct law_ht_creq *request);

/**
 * Get the client-side request's heap.
 */
struct pgc_stk *law_http_creq_heap(
        struct law_ht_creq *request);

/**
 * Establish a network connection with an HTTP service.
 */
sel_err_t law_http_creq_open(
        struct law_ht_creq *request, 
        const struct sockaddr *address, 
        socklen_t address_length);

/**
 * Write the client-side request's head.
 */
sel_err_t law_http_creq_write_head(
        struct law_ht_creq *request,
        enum law_ht_method request_method,
        const char *request_target,
        const size_t header_count,
        const char *headers[][2]);

/**
 * Write data from the output buffer.
 */
ssize_t law_http_creq_write_data(
        struct law_ht_creq *request);

/**
 * Read the client-side request's response head.
 */
sel_err_t law_http_creq_read_head(
        struct law_ht_creq *request,
        const int status,
        struct law_ht_hdrs **headers);

/**
 * Read bytes into the input buffer.
 */
ssize_t law_http_creq_read_data(
        struct law_ht_creq *request);

/**
 * Finish the client-side request.
 */
sel_err_t law_http_creq_done(
        struct law_ht_creq *request);

/** 
 * Get status code description. 
 */
const char *law_http_status_str(
        const int status_code);

/** 
 * Create a new HTTP state. 
 */
struct law_ht_sctx *law_http_sctx_create(
        struct law_ht_sctx_cfg *configuration);

/** 
 * Destroy HTTP state. 
 */
void law_http_sctx_destroy(
        struct law_ht_sctx *http);

/**
 * Entry function for HTTP functionality.
 */
sel_err_t law_http_accept(
        struct law_srv *server,
        int socket,
        void *state);


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
 * start_line = method SP request_target SP HTTP_version CRLF
 * 
 * field_name = token
 * 
 * VCHAR = "any visible USASCII character"
 * 
 * SP = ' '
 * 
 * HTAB = '\t' 
 * 
 * OWS = *( SP / HTAB )
 * 
 * field_vchar = VCHAR
 * 
 * field_content  = field_vchar [ 1*( SP | HTAB ) field_vchar ]
 * 
 * field_value = *( field_content )
 * 
 * header_field = field_name ":" OWS field_value OWS
 * 
 * HTTP_message = start_line
 *              *( header_field CRLF )
 *              CRLF
 *              [ message_body ]
 */

enum pgc_err law_http_cap_method(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_version(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_field_name(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_field_value(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_field(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_origin_form(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_absolute_form(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_authority_form(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

enum pgc_err law_http_cap_asterisk_form(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

/** Export and Link */
struct law_http_parsers *law_http_parsers_link();

/* Auto-Generated Parsers */

struct law_http_parsers;

struct law_http_parsers *export_law_http_parsers();

struct pgc_par *law_http_parsers_tchar(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_token(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_request_target(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_start_line(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_field_content(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_field_value(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_header_field(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_HTTP_message_head(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_cap_absolute_URI(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_cap_origin_URI(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_cap_authority(struct law_http_parsers *x);

#endif