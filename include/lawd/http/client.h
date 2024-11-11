#ifndef LAWD_HTTP_CLIENT_H
#define LAWD_HTTP_CLIENT_H

#include "lawd/http/conn.h"

/** HTTP Client */
struct law_htclient;

struct law_htcl_head {
    int aslkdjfl;
};

struct law_htcl_cfg {
    int asdfasdf;
};

/** Create a new HTTP client. */
struct law_htclient *law_htcl_create(struct law_htcl_cfg *cfg);

/** Destroy the client request. */
void law_htcl_destroy(struct law_htclient *request);

/** Get the client-side request's heap. */
struct pgc_stk *law_htcl_heap(struct law_htclient *request);

/** Establish a network connection with an HTTP service. */
sel_err_t law_htcl_connect(
        struct law_htclient *request, 
        const char *host,
        const char *service);

/** Write the request-line */
sel_err_t law_htcl_reqline(
        struct law_htclient *request,
        const char *method,
        const char *target,
        const char *version);

/** Write a header. */
sel_err_t law_htcl_header(
        struct law_htclient *request,
        const char *field_name,
        const char *field_value);

/** Begin writing the request body. */
sel_err_t law_htcl_body(struct law_htclient *request);

/** Write data from the output buffer. */
sel_err_t law_htcl_write_data(struct law_htclient *request);

/** Send the request. */
sel_err_t law_htcl_send(struct law_htclient *request);

/** Read bytes into the input buffer. */
ssize_t law_htcl_read_data(struct law_htclient *request);

/** Read the client-side request's response head. */
sel_err_t law_htcl_read_head(
        struct law_htclient *request,
        struct law_htcl_head *head);

/** Done reading response. */
sel_err_t law_htcl_done(struct law_htclient *request);

#endif