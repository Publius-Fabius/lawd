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
        LAW_HTTP_GET,
        LAW_HTTP_POST,
        LAW_HTTP_HEAD,
        LAW_HTTP_PUT,
        LAW_HTTP_DELETE,
        LAW_HTTP_PATCH,
        LAW_HTTP_OPTIONS,
        LAW_HTTP_TRACE,
        LAW_HTTP_CONNECT
};

/** HTTP Version */
enum law_http_version {
        LAW_HTTP_1_1,
        LAW_HTTP_2
};

/** HTTP Request */
struct law_http_req;

/** HTTP Response */
struct law_http_res;

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
enum law_http_method law_http_getmethod(struct law_http_req *req);

/** 
 * Get the request's HTTP version.
 */
enum law_http_version law_http_getversion(struct law_http_req *req);

/**
 * Get the request's URI.
 */
struct law_uri *law_http_geturi(struct law_http_req *req);

/** Create a new HTTP state. */
struct law_http law_http_create(struct law_http_cfg *cfg);

/** Destroy HTTP state. */
void law_http_destroy(struct law_http *http);

/**
 * Entry function for HTTP/S functionality.
 */
sel_err_t law_http_accept(
        struct law_srv *server,
        int socket,
        void *state);

/** HTTP Parser Dictionary */
struct law_http_parsers;

/** Link */
void link_http_parsers();

/** Export HTTP Parsers */
struct law_http_parsers *export_law_http_parsers();

struct pgc_par *law_http_parsers_tchar(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_token(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_method(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_path_absolute(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_absolute_URI(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_authority(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_query(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_origin_form(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_absolute_form(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_authority_form(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_asterisk_form(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_field_value(struct law_http_parsers *x);

struct pgc_par *law_http_parsers_header_field(struct law_http_parsers *x);

#endif