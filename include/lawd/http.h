#ifndef LAWD_HTTP_H
#define LAWD_HTTP_H

#include "lawd/server.h"
#include "lawd/uri.h"

/**
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

/** HTTP Method */
enum law_http_method {
        LAW_HTTP_GET,                   /** HTTP Get Method */
        LAW_HTTP_POST,                  /** HTTP Post Method */
        LAW_HTTP_HEAD,                  /** HTTP Head Method */
        LAW_HTTP_PUT,                   /** HTTP Put Method */
        LAW_HTTP_DELETE,                /** HTTP Delete Method */
        LAW_HTTP_PATCH,                 /** HTTP Patch Method */
        LAW_HTTP_OPTIONS,               /** HTTP Options Method */
        LAW_HTTP_TRACE,                 /** HTTP Trace Method */
        LAW_HTTP_CONNECT                /** HTTP Connect Method */
};

/** HTTP Version */
enum law_http_version {
        LAW_HTTP_1_1,                   /** HTTP Version 1.1 */
        LAW_HTTP_2                      /** HTTP Version 2 */
};

/** HTTP Message Part */
enum law_http_part {
        LAW_HTTP_METHOD,                /** Method */
        LAW_HTTP_VERSION,               /** Version */
        LAW_HTTP_FIELD,                 /** Header Field */
        LAW_HTTP_FIELD_NAME,            /** Header Field Name */
        LAW_HTTP_FIELD_VALUE,           /** Header Field Value */
        LAW_HTTP_ORIGIN_FORM,           /** Origin URI Form */
        LAW_HTTP_ABSOLUTE_FORM,         /** Absolute URI Form */
        LAW_HTTP_AUTHORITY_FORM         /** Authority URI Form */
};

/** HTTP Request */
struct law_http_req;

/** HTTP Response */
struct law_http_res;

/** HTTP Headers */
struct law_http_hdrs;

/** HTTP Headers Iterator */
struct law_http_hdrs_iter;

/** HTTP Input Stream */
struct law_http_istream;

/** HTTP Output Stream */
struct law_http_ostream;

/** 
 * Callback type for handling requests.
 * @param server The server.
 * @param request The request.
 * @param response The response. 
 */
typedef sel_err_t (*law_http_handler_t)(
        struct law_srv *server,
        struct law_http_req *request,
        struct law_http_res *response);

/** HTTP Configuration */
struct law_http_cfg {
        enum law_http_version version;          /** HTTP Version */
        size_t in_length;                       /** Input Buffer Length */
        size_t in_guard;                        /** Input Buffer Guard */
        size_t out_length;                      /** Output Buffer Length */
        size_t out_guard;                       /** Output Buffer Guard */
        size_t heap_length;                     /** Heap Length */
        size_t heap_guard;                      /** Heap Guard */
        law_http_handler_t handler;             /** Handler Callback */
};

/** HTTP State */
struct law_http;

/**
 * Get the request's method.
 */
enum law_http_method law_http_req_method(
        struct law_http_req *request);

/** 
 * Get the request's HTTP version.
 */
enum law_http_version law_http_req_version(
        struct law_http_req *request);

/**
 * Get the request's URI.
 */
struct law_uri *law_http_req_uri(
        struct law_http_req *request);

/**
 * Get the request's headers.
 */
struct law_http_hdrs *law_http_req_headers(
        struct law_http_req *request);

/**
 * Get the header value by name.
 */
const char *law_http_hdrs_lookup(
        struct law_http_hdrs *headers,
        const char *name);

/**
 * Get an iterator for the header collection.
 */
struct law_http_hdrs_iter *law_http_hdrs_elems(
        struct law_http_hdrs *headers);

/**
 * Free the header iterator.
 */
void law_http_hdrs_iter_free(
        struct law_http_hdrs_iter *iterator);

/**
 * Get the next header field <name, value> pair.
 */
struct law_http_hdrs_iter *law_http_hdrs_iter_next(
        struct law_http_hdrs_iter *iterator,
        const char **name,
        const char **value);

/**
 * Get an input_stream limited to content_length bytes.
 */
struct law_http_istream *law_http_req_body(
        struct law_http_req *request,
        const size_t content_length);

/**
 * Setup the request to read Content-Type: multipart/form-data.
 */
sel_err_t law_http_req_multipart_form_data(
        struct law_http_req *request,
        const char *boundary);

/**
 * Get multipart/form-data section headers.
 */
struct law_http_hdrs *law_http_req_multipart_form_data_headers(
        struct law_http_req *request);

/**
 * Get multipart/form-data body.
 */
struct law_http_istream *law_http_req_multipart_form_data_body(
        struct law_http_req *request);

/* ISTREAM SECTION */

/**
 * Get a view of the input_stream's available bytes.
 */
struct pgc_buf *law_http_istream_view(
        struct pgc_buf *buffer,
        struct law_http_istream *input_stream);

/**
 * Flush n-bytes of stream data.
 */
sel_err_t law_http_istream_flush(
        struct law_http_istream *input_stream,
        const size_t nbytes);

/* RESPONSE SECTION */

/** Get status code description. */
const char *law_http_res_code(const int code);

/**
 * Set response status code.
 */
sel_err_t law_http_res_status(
        struct law_http_res *response);

/**
 * Include the header in the response.
 */
sel_err_t law_http_res_header(
        struct law_http_res *response,
        const char *name,
        const char *value);

/**
 * Get a stream for the response's body.
 */
struct law_http_ostream *law_http_res_body(
        struct law_http_res *response);

/**
 * Get a view of the output_stream's available bytes.
 */
struct pgc_buf *law_http_ostream_view(
        struct pgc_buf *buffer,
        struct law_http_ostream *output_stream);

/**
 * Flush n-bytes of stream data.
 */
sel_err_t law_http_ostream_flush(
        struct law_http_ostream *output_stream,
        const size_t nbytes);

/** 
 * Create a new HTTP state. 
 */
struct law_http *law_http_create(
        struct law_http_cfg *configuration);

/** 
 * Destroy HTTP state. 
 */
void law_http_destroy(
        struct law_http *http);

/**
 * Entry function for HTTP/S functionality.
 */
sel_err_t law_http_accept(
        struct law_srv *server,
        int socket,
        void *state);

/* PARSER SECTION */

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

/** Link */
void law_http_parsers_link();

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