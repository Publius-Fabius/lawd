
#include "lawd/http.h"
#include "lawd/safemem.h"
#include "pgenc/lang.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <openssl/types.h>

struct law_http {
        struct law_http_cfg *cfg;
};

struct law_http_hdrs {
        struct pgc_ast_lst *list;
};

struct law_http_hdrs_iter {
        struct pgc_ast_lst *list;
};

struct law_http_req {
        int socket;
        enum law_http_method method;
        enum law_http_version version;
        struct law_uri *uri;
        struct law_http_hdrs *headers;
        struct pgc_buf *in;
        struct pgc_stk *heap;
        uint64_t uid;
        SSL *ssl;
};

struct law_http_res {
        struct pgc_buf *out;
        SSL *ssl;
};

enum law_http_method law_http_req_method(struct law_http_req *request)
{
        return request->method;
}

enum law_http_version law_http_req_version(struct law_http_req *request)
{
        return request->version;
}

struct law_uri *law_http_req_uri(struct law_http_req *request)
{
        return request->uri;
}

struct law_http_hdrs *law_http_req_headers(struct law_http_req *req)
{
        return req->headers;
}

const char *law_http_hdrs_lookup(
        struct law_http_hdrs *hdrs,
        const char *name)
{
        static const char *EMPTY = "";
        for(struct pgc_ast_lst *l = hdrs->list; l; l = l->nxt) {
                struct pgc_ast_lst *tuple = pgc_ast_tolst(l->val);
                if(strcmp(pgc_ast_tostr(tuple->val), name)) {
                        continue;
                } else if(tuple->nxt) {
                        return pgc_ast_tostr(tuple->nxt->val);
                } else {
                        return EMPTY;
                }
        }
        return NULL;
}

struct law_http_hdrs_iter *law_http_hdrs_elems(struct law_http_hdrs *hdrs)
{
        struct law_http_hdrs_iter *iter = 
                malloc(sizeof(struct law_http_hdrs_iter));
        iter->list = hdrs->list;
        return iter;
}

void law_http_hdrs_iter_free(struct law_http_hdrs_iter *iter)
{
        free(iter);
}

struct law_http_hdrs_iter *law_http_hdrs_iter_next(
        struct law_http_hdrs_iter *iter,
        const char **name,
        const char **value)
{
        static const char *EMPTY = "";
        if(!iter->list) {
                law_http_hdrs_iter_free(iter);
                return NULL;
        }
        struct pgc_ast_lst *lst = pgc_ast_tolst(iter->list->val);
        *name = pgc_ast_tostr(lst->val);
        if(lst->nxt) {
                *value = pgc_ast_tostr(lst->nxt->val);
        } else {
                *value = EMPTY;
        }
        iter->list = iter->list->nxt;
        return iter;
}

const char *law_http_code(const int code)
{
        switch(code) {

                /* Informational 1xx */
                case 100: return "Continue";
                case 101: return "Switching Protocols";

                /* Successful 2xx */
                case 200: return "OK";
                case 201: return "Created";
                case 202: return "Accepted";
                case 203: return "Non-Authoritative Information";
                case 204: return "No Content";
                case 205: return "Reset Content";

                /* Redirection 3xx */
                case 300: return "Multiple Choices";
                case 301: return "Moved Permanently";
                case 302: return "Found";
                case 303: return "See Other";
                case 304: return "Not Modified";
                case 305: return "Use Proxy";
                case 307: return "Temporary Redirect";

                /* Client Error 4xx */
                case 400: return "Bad Request";
                case 401: return "Unauthorized";
                case 402: return "Payment Required";
                case 403: return "Forbidden";
                case 404: return "Not Found";
                case 405: return "Method Not Allowed";
                case 406: return "Not Acceptable";
                case 407: return "Proxy Authentication Required";
                case 408: return "Request Timeout";
                case 409: return "Conflict";
                case 410: return "Gone";
                case 411: return "Length Required";
                case 412: return "Precondition Failed";
                case 413: return "Request Entity Too Large";
                case 414: return "Request-URI Too Long";
                case 415: return "Unsupported Media Type";
                case 416: return "Requested Range Not Satisfiable";
                case 417: return "Expectation Failed";

                /* Server Error 5xx */
                case 500: return "Internal Server Error";
                case 501: return "Not Implemented";
                case 502: return "Bad Gateway";
                case 503: return "Service Unavailable";
                case 504: return "Gateway Timeout";
                case 505: return "HTTP Version Not Supported";

                default: return NULL;
        }
}

sel_err_t law_http_res_status(
        struct law_http_res *response);

sel_err_t law_http_res_header(
        struct law_http_res *response,
        const char *name,
        const char *value);

struct law_http_ostream *law_http_res_body(
        struct law_http_res *response);

struct pgc_buf *law_http_ostream_view(
        struct pgc_buf *buffer,
        struct law_http_ostream *output_stream);

sel_err_t law_http_ostream_flush(
        struct law_http_ostream *output_stream,
        const size_t nbytes);

struct law_http *law_http_create(struct law_http_cfg *cfg)
{
        struct law_http *http = malloc(sizeof(struct law_http));
        http->cfg = cfg;
        return http;
}

void law_http_destroy(struct law_http *http)
{
        free(http);
}

sel_err_t law_http_entry_launch(
        struct law_srv *server,
        struct law_http *http,
        struct law_http_req *req,
        struct law_http_res *res)
{
                return 0;
}

sel_err_t law_http_accept(
        struct law_srv *srv,
        int sock,
        void *state)
{
        struct law_http *http = state;
        const size_t in_length = http->cfg->in_length;
        const size_t in_guard = http->cfg->in_guard;
        const size_t out_length = http->cfg->out_length;
        const size_t out_guard = http->cfg->out_guard;
        const size_t heap_length = http->cfg->heap_length;
        const size_t heap_guard = http->cfg->heap_guard;

        struct law_smem *in_mem = law_smem_create(in_length, in_guard);
        struct law_smem *out_mem = law_smem_create(in_length, in_guard);
        struct law_smem *heap_mem = law_smem_create(heap_length, heap_guard);

        struct pgc_stk heap;
        pgc_stk_init(&heap, heap_length, law_smem_address(heap_mem));

        struct pgc_buf in, out;
        pgc_buf_init(&in, law_smem_address(in_mem), in_length, 0);
        pgc_buf_init(&out, law_smem_address(out_mem), out_length, 0);

        struct law_http_req req;
        req.socket = sock;
        req.in = &in;
        req.heap = &heap;

        struct law_http_res res;
        res.out = &out;

        sel_err_t error = law_http_entry_launch(srv, http, &req, &res);

        law_smem_destroy(heap_mem);
        law_smem_destroy(out_mem);
        law_smem_destroy(in_mem);

        return error;
}       

enum pgc_err law_http_cap_method(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HTTP_METHOD, pgc_lang_readstr);
}

enum pgc_err law_http_cap_version(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HTTP_VERSION, pgc_lang_readstr);
}

enum pgc_err law_http_cap_field_name(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HTTP_FIELD_NAME, pgc_lang_readstr);
}

enum pgc_err law_http_cap_field_value(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HTTP_FIELD_VALUE, pgc_lang_readstr);
}

enum pgc_err law_http_cap_field(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buffer, state, arg, LAW_HTTP_FIELD, 0);
}

enum pgc_err law_http_cap_origin_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HTTP_ORIGIN_FORM, 0);
}

enum pgc_err law_http_cap_absolute_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HTTP_ABSOLUTE_FORM, 0);
}

enum pgc_err law_http_cap_authority_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HTTP_AUTHORITY_FORM, 0);
}

void law_http_parsers_link()
{
        /*      dec cap_absolute_URI;
                dec cap_origin_URI;
                dec cap_authority; */

        struct law_uri_parsers *urips = export_law_uri_parsers();
        struct law_http_parsers *https = export_law_http_parsers();

        struct pgc_par *abs_URI = law_uri_parsers_cap_absolute_URI(urips);
        struct pgc_par *ori_URI = law_uri_parsers_cap_origin_URI(urips);
        struct pgc_par *cap_auth = law_uri_parsers_cap_authority(urips);

        struct pgc_par *abs_URI_l = law_http_parsers_cap_absolute_URI(https);
        struct pgc_par *ori_URI_l = law_http_parsers_cap_origin_URI(https);
        struct pgc_par *cap_auth_l = law_http_parsers_cap_authority(https);

        abs_URI_l->u.lnk = abs_URI;
        ori_URI_l->u.lnk = ori_URI;
        cap_auth_l->u.lnk = cap_auth;
}
