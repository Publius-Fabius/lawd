#ifndef LAWD_HTTP_H
#define LAWD_HTTP_H

#include "lawd/server.h"
#include "lawd/uri.h"

/**
 *  RFC7230 - Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
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

#endif