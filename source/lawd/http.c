
#include "lawd/http.h"
#include "lawd/safemem.h"
#include "pgenc/lang.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <openssl/types.h>

struct law_http_hdrs {
        struct pgc_ast_lst *list;
};

struct law_http_hdrs_i {
        struct pgc_ast_lst *list;
};

struct law_http_sctx {
        struct law_http_sctx_cfg *cfg;
};

struct law_http_sreq {
        int socket;
        enum law_http_method method;
        enum law_http_version version;
        struct law_uri *uri;
        struct law_http_hdrs *headers;
        struct pgc_buf *in;
        struct pgc_buf *out;
        struct pgc_stk *heap;
        SSL *ssl;
};

struct law_http_creq {
        int socket;
        int status;
        struct law_http_hdrs *headers;
        struct pgc_buf *in;
        struct pgc_buf *out;
        struct pgc_stk *heap;
        SSL *ssl;
};

const char *law_http_hdrs_get(
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

struct law_http_hdrs_i *law_http_hdrs_elems(struct law_http_hdrs *hdrs)
{
        struct law_http_hdrs_i *it = malloc(sizeof(struct law_http_hdrs_i));
        it->list = hdrs->list;
        return it;
}

void law_http_hdrs_i_free(struct law_http_hdrs_i *iter)
{

}

struct law_http_hdrs_i *law_http_hdrs_i_next(
        struct law_http_hdrs_i *iter,
        const char **name,
        const char **value)
{

}

struct pgc_buf *law_http_sreq_in(struct law_http_sreq *req)
{
        return req->in;
}

struct pgc_buf *law_http_sreq_out(struct law_http_sreq *req)
{
        return req->out;
}

struct pgc_stk *law_http_sreq_heap(struct law_http_sreq *req)
{
        return req->heap;
}

enum law_http_method law_http_sreq_method(struct law_http_sreq *req)
{
        return req->method;
}

enum law_http_version law_http_sreq_version(struct law_http_sreq *req)
{
        return req->version;
}

struct law_uri *law_http_sreq_uri(struct law_http_sreq *req)
{
        return req->uri;
}

struct law_http_hdrs *law_http_sreq_headers(struct law_http_sreq *req)
{
        return req->headers;
}


// struct law_http_hdrs_i *law_http_hdrs_elems(struct law_http_hdrs *hdrs)
// {
//         struct law_http_hdrs_i *iter = 
//                 malloc(sizeof(struct law_http_hdrs_i));
//         iter->list = hdrs->list;
//         return iter;
// }

// void law_http_hdrs_i_free(struct law_http_hdrs_i *iter)
// {
//         free(iter);
// }

// struct law_http_hdrs_i *law_http_hdrs_i_next(
//         struct law_http_hdrs_i *iter,
//         const char **name,
//         const char **value)
// {
//         static const char *EMPTY = "";
//         if(!iter->list) {
//                 law_http_hdrs_i_free(iter);
//                 return NULL;
//         }
//         struct pgc_ast_lst *lst = pgc_ast_tolst(iter->list->val);
//         *name = pgc_ast_tostr(lst->val);
//         if(lst->nxt) {
//                 *value = pgc_ast_tostr(lst->nxt->val);
//         } else {
//                 *value = EMPTY;
//         }
//         iter->list = iter->list->nxt;
//         return iter;
// }

// const char *law_http_code(const int code)
// {
//         switch(code) {

//                 /* Informational 1xx */
//                 case 100: return "Continue";
//                 case 101: return "Switching Protocols";

//                 /* Successful 2xx */
//                 case 200: return "OK";
//                 case 201: return "Created";
//                 case 202: return "Accepted";
//                 case 203: return "Non-Authoritative Information";
//                 case 204: return "No Content";
//                 case 205: return "Reset Content";

//                 /* Redirection 3xx */
//                 case 300: return "Multiple Choices";
//                 case 301: return "Moved Permanently";
//                 case 302: return "Found";
//                 case 303: return "See Other";
//                 case 304: return "Not Modified";
//                 case 305: return "Use Proxy";
//                 case 307: return "Temporary Redirect";

//                 /* Client Error 4xx */
//                 case 400: return "Bad Request";
//                 case 401: return "Unauthorized";
//                 case 402: return "Payment Required";
//                 case 403: return "Forbidden";
//                 case 404: return "Not Found";
//                 case 405: return "Method Not Allowed";
//                 case 406: return "Not Acceptable";
//                 case 407: return "Proxy Authentication Required";
//                 case 408: return "Request Timeout";
//                 case 409: return "Conflict";
//                 case 410: return "Gone";
//                 case 411: return "Length Required";
//                 case 412: return "Precondition Failed";
//                 case 413: return "Request Entity Too Large";
//                 case 414: return "Request-URI Too Long";
//                 case 415: return "Unsupported Media Type";
//                 case 416: return "Requested Range Not Satisfiable";
//                 case 417: return "Expectation Failed";

//                 /* Server Error 5xx */
//                 case 500: return "Internal Server Error";
//                 case 501: return "Not Implemented";
//                 case 502: return "Bad Gateway";
//                 case 503: return "Service Unavailable";
//                 case 504: return "Gateway Timeout";
//                 case 505: return "HTTP Version Not Supported";

//                 default: return NULL;
//         }
// }

// struct pgc_buf *law_http_res_output(
//         struct law_http_res *response)
// {
//         return response->out;
// }

// sel_err_t law_http_res_status(
//         struct law_http_res *response)
// {
//         SEL_ABORT();
// }

// sel_err_t law_http_res_header(
//         struct law_http_res *response,
//         const char *name,
//         const char *value)
// {
//         SEL_ABORT();
// }

// struct law_http_sctx *law_http_sctx_create(struct law_http_sctx_cfg *cfg)
// {
//         struct law_http_sctx *http = malloc(sizeof(struct law_http_sctx));
//         http->cfg = cfg;
//         return http;
// }

// void law_http_sctx_destroy(struct law_http_sctx *http)
// {
//         free(http);
// }

// static enum law_http_method law_http_find_method(const char *str)
// {
//         if(!strcmp(str, "GET")) {
//                 return LAW_HTTP_GET;
//         } else if(!strcmp(str, "POST")) {
//                 return LAW_HTTP_POST;
//         } else if(!strcmp(str, "HEAD")) {
//                 return LAW_HTTP_HEAD;
//         } else if(!strcmp(str, "PUT")) {
//                 return LAW_HTTP_PUT;
//         } else if(!strcmp(str, "DELETE")) {
//                 return LAW_HTTP_DELETE;
//         } else if(!strcmp(str, "PATCH")) {
//                 return LAW_HTTP_PATCH;
//         } else if(!strcmp(str, "OPTIONS")) {
//                 return LAW_HTTP_OPTIONS;
//         } else if(!strcmp(str, "TRACE")) {
//                 return LAW_HTTP_TRACE;
//         } else if(!strcmp(str, "CONNECT")) {
//                 return LAW_HTTP_CONNECT;
//         } else {
//                 return -1;
//         }
// }

// static enum law_http_version law_http_find_version(const char *str)
// {
//         if(!strcmp(str, "HTTP/1.1")) {
//                 return LAW_HTTP_1_1;
//         } else if(!strcmp(str, "HTTP/2")) {
//                 return LAW_HTTP_2;
//         } else {
//                 return -1;
//         }
// }

// static sel_err_t law_http_launch(
//         struct law_srv *server,
//         struct law_http_sctx *http,
//         struct law_http_req *request,
//         struct law_http_res *response)
// {
//         int socket = request->socket;
//         struct pgc_buf *in = request->in;
//         struct pgc_stk *heap = request->heap;
//         const size_t MAX = pgc_buf_max(in);
//         enum pgc_err err;
        
//         /* Find the end of the HTTP message's head.*/
//         for(;;) {

//                 const size_t end = pgc_buf_end(in);
//                 if(end == MAX) {
//                         return LAW_HTTP_FAIL(
//                                 server, 
//                                 http, 
//                                 request, 
//                                 response, 
//                                 400, 
//                                 LAW_ERR_BUFFER_LIMIT);
//                 }

//                 err = pgc_buf_seek(in, 0);

//                 SEL_ASSERT(err == PGC_ERR_OK);
//                 SEL_ASSERT(end <= MAX);

//                 err = pgc_buf_read(in, socket, MAX - end);
                
//                 if(err == PGC_ERR_SYS) {
//                         if(errno == EAGAIN || errno == EWOULDBLOCK) {
//                                 law_srv_poll(
//                                         server, 
//                                         socket, 
//                                         POLLIN | POLLHUP);
//                                 continue;
//                         } else {
//                                 return LAW_HTTP_FAIL(
//                                         server, 
//                                         http, 
//                                         request, 
//                                         response, 
//                                         500, 
//                                         LAW_ERR_SOCKET_IO);
//                         }
//                 } else if(err == PGC_ERR_EOF) {
//                         return LAW_HTTP_FAIL(
//                                 server, 
//                                 http, 
//                                 request, 
//                                 response, 
//                                 400, 
//                                 LAW_ERR_ENCOUNTERED_EOF);
//                 } 

//                 SEL_ASSERT(err == PGC_ERR_OK);

//                 err = pgc_buf_seek(in, end - 3);

//                 SEL_ASSERT(err == PGC_ERR_OK);

//                 err = pgc_buf_scan(in, "\r\n\r\n", 4);
//                 if(err == PGC_ERR_OK) {
//                         break;
//                 } 

//                 SEL_ASSERT(err == PGC_ERR_OOB);
//         }

//         /* Parse the HTTP message's head. */

//         const size_t head_end = pgc_buf_tell(in);
//         err = pgc_buf_seek(in, 0);

//         SEL_ASSERT(err == PGC_ERR_OK);

//         struct pgc_buf lens; 
//         pgc_buf_lens(&lens, in, head_end);

//         struct law_http_parsers *dict = law_http_parsers_link();
//         struct pgc_par *head_par = law_http_parsers_HTTP_message_head(dict);

//         struct pgc_ast_lst *head;

//         err = pgc_lang_parse(head_par, &lens, heap, &head);
//         if(err != PGC_ERR_OK) {
//                 return LAW_HTTP_FAIL(
//                         server, 
//                         http, 
//                         request, 
//                         response, 
//                         400, 
//                         LAW_ERR_MALFORMED_MESSAGE);
//         }

//         /* Extract data from the parse result. */

//         const enum law_http_method method = law_http_find_method(
//                 pgc_ast_tostr(pgc_ast_at(head, 0)->val));

//         if(method == -1) {
//                 return LAW_HTTP_FAIL(
//                         server,
//                         http,
//                         request,
//                         response,
//                         405,
//                         LAW_ERR_METHOD_NOT_ALLOWED);
//         }

//         struct law_uri uri;
//         law_uri_from_ast(&uri, pgc_ast_tolst(pgc_ast_at(head, 1)->val));
        
//         const enum law_http_version version = law_http_find_version(
//                 pgc_ast_tostr(pgc_ast_at(head, 2)->val));

//         if(version == -1) {
//                 return LAW_HTTP_FAIL(
//                         server,
//                         http,
//                         request,
//                         response,
//                         505,
//                         LAW_ERR_UNSUPPORTED_VERSION);
//         }

//         struct law_http_hdrs hdrs;
//         hdrs.list = pgc_ast_at(head, 3);

//         request->headers = &hdrs;
//         request->uri = &uri;
//         request->method = method;
//         request->version = version;
        
//         return http->cfg->onaccept(server, http, request, response);
// }

// sel_err_t law_http_accept(
//         struct law_srv *srv,
//         int sock,
//         void *state)
// {
//         struct law_http_sctx *http = state;
//         const size_t in_length = http->cfg->in_length;
//         const size_t in_guard = http->cfg->in_guard;
//         const size_t out_length = http->cfg->out_length;
//         const size_t out_guard = http->cfg->out_guard;
//         const size_t heap_length = http->cfg->heap_length;
//         const size_t heap_guard = http->cfg->heap_guard;

//         struct law_smem *in_mem = law_smem_create(in_length, in_guard);
//         struct law_smem *out_mem = law_smem_create(out_length, out_guard);
//         struct law_smem *heap_mem = law_smem_create(heap_length, heap_guard);

//         struct pgc_stk heap;
//         pgc_stk_init(&heap, heap_length, law_smem_address(heap_mem));

//         struct pgc_buf in, out;
//         pgc_buf_init(&in, law_smem_address(in_mem), in_length, 0);
//         pgc_buf_init(&out, law_smem_address(out_mem), out_length, 0);

//         struct law_http_req req;
//         req.socket = sock;
//         req.in = &in;
//         req.heap = &heap;

//         struct law_http_res res;
//         res.out = &out;

//         sel_err_t error = law_http_launch(srv, http, &req, &res);

//         law_smem_destroy(heap_mem);
//         law_smem_destroy(out_mem);
//         law_smem_destroy(in_mem);

//         return error;
// }       

// sel_err_t law_http_onfail(
//         struct law_srv *server,
//         struct law_http_sctx *http,
//         struct law_http_req *request,
//         struct law_http_res *response,
//         const int status_code,
//         sel_err_t error_type,
//         const char *file,
//         const char *func,
//         const int line)
// {
//         fprintf(http->cfg->error_log, 
//                 "%i:%s:%s",
//                 status_code,
//                 law_http_status_str(status_code),
//                 sel_lookup(error_type));
        
//         if(error_type == LAW_ERR_SOCKET_IO) {
//                 fprintf(http->cfg->error_log, 
//                         ":%s:%s:%s:%i\r\n", 
//                         strerror(errno),
//                         file,
//                         func,
//                         line);
//                 close(request->socket);
//                 return SEL_ERR_OK;
//         } else if(error_type == LAW_ERR_MALFORMED_MESSAGE) {
//                 fprintf(http->cfg->error_log, 
//                         ":%zi", 
//                         pgc_buf_tell(request->in));
//         }

//         fprintf(http->cfg->error_log, 
//                 "%s:%s:%i\r\n",
//                 file,
//                 func,
//                 line);

//         return SEL_ERR_OK;
// }

// enum pgc_err law_http_cap_method(
//         struct pgc_buf *buffer,
//         void *state,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readterm(
//                 buffer, state, arg, LAW_HTTP_METHOD, pgc_lang_readstr);
// }

// enum pgc_err law_http_cap_version(
//         struct pgc_buf *buffer,
//         void *state,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readterm(
//                 buffer, state, arg, LAW_HTTP_VERSION, pgc_lang_readstr);
// }

// enum pgc_err law_http_cap_field_name(
//         struct pgc_buf *buffer,
//         void *state,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readterm(
//                 buffer, state, arg, LAW_HTTP_FIELD_NAME, pgc_lang_readstr);
// }

// enum pgc_err law_http_cap_field_value(
//         struct pgc_buf *buffer,
//         void *state,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readterm(
//                 buffer, state, arg, LAW_HTTP_FIELD_VALUE, pgc_lang_readstr);
// }

// enum pgc_err law_http_cap_field(
//         struct pgc_buf *buffer,
//         void *state,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readexp(buffer, state, arg, LAW_HTTP_FIELD, 0);
// }

// enum pgc_err law_http_cap_origin_form(
//         struct pgc_buf *buf,
//         void *st,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readexp(buf, st, arg, LAW_HTTP_ORIGIN_FORM, 0);
// }

// enum pgc_err law_http_cap_absolute_form(
//         struct pgc_buf *buf,
//         void *st,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readexp(buf, st, arg, LAW_HTTP_ABSOLUTE_FORM, 0);
// }

// enum pgc_err law_http_cap_authority_form(
//         struct pgc_buf *buf,
//         void *st,
//         struct pgc_par *arg)
// {
//         return pgc_lang_readexp(buf, st, arg, LAW_HTTP_AUTHORITY_FORM, 0);
// }

// struct law_http_parsers *law_http_parsers_link()
// {
//         /*      dec cap_absolute_URI;
//                 dec cap_origin_URI;
//                 dec cap_authority; */

//         static struct law_http_parsers *https = NULL;
//         static struct law_uri_parsers *urips = NULL;

//         if(https) {
//                 return https;
//         }

//         https = export_law_http_parsers();
//         urips = export_law_uri_parsers();

//         struct pgc_par *abs_URI = law_uri_parsers_cap_absolute_URI(urips);
//         struct pgc_par *ori_URI = law_uri_parsers_cap_origin_URI(urips);
//         struct pgc_par *cap_auth = law_uri_parsers_cap_authority(urips);

//         struct pgc_par *abs_URI_l = law_http_parsers_cap_absolute_URI(https);
//         struct pgc_par *ori_URI_l = law_http_parsers_cap_origin_URI(https);
//         struct pgc_par *cap_auth_l = law_http_parsers_cap_authority(https);

//         abs_URI_l->u.lnk = abs_URI;
//         ori_URI_l->u.lnk = ori_URI;
//         cap_auth_l->u.lnk = cap_auth;

//         return https;
// }
