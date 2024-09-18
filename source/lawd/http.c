
#define _POSIX_C_SOURCE 200112L

#include "lawd/http.h"
#include "lawd/safemem.h"
#include "pgenc/lang.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <openssl/types.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

struct law_ht_hdrs {
        struct pgc_ast_lst *list;
};

struct law_ht_hdrs_i {
        struct pgc_ast_lst *list;
};

/** HTTP Connection State */
struct law_ht_conn {
        int socket;
        SSL *ssl;
        struct pgc_buf *in;
        struct pgc_buf *out;
};

struct law_ht_sreq {
        struct law_ht_conn conn;
        size_t scan;
        struct pgc_stk *heap;
};

struct law_ht_creq {
        struct law_ht_conn conn;
        size_t scan;
        struct pgc_stk *heap;
        struct law_smem *in_mem;
        struct law_smem *out_mem;
        struct law_smem *heap_mem;  
};

struct law_ht_sctx {
        struct law_ht_sctx_cfg *cfg;
};

/* HEADERS SECTION ##########################################################*/

const char *law_ht_hdrs_get(
        struct law_ht_hdrs *hdrs,
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

struct law_ht_hdrs_i *law_ht_hdrs_elems(struct law_ht_hdrs *hdrs)
{
        struct law_ht_hdrs_i *it = malloc(sizeof(struct law_ht_hdrs_i));
        it->list = hdrs->list;
        return it;
}

void law_ht_hdrs_i_free(struct law_ht_hdrs_i *iter)
{
        return free(iter);
}

struct law_ht_hdrs_i *law_ht_hdrs_i_next(
        struct law_ht_hdrs_i *iter,
        const char **name,
        const char **value)
{
        static const char *EMPTY = "";
        if(!iter->list) {
                law_ht_hdrs_i_free(iter);
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

/* SERVER REQUEST SECTION ###################################################*/

struct pgc_buf *law_ht_sreq_in(struct law_ht_sreq *req)
{
        return req->conn.in;
}

struct pgc_buf *law_ht_sreq_out(struct law_ht_sreq *req)
{
        return req->conn.out;
}

struct pgc_stk *law_ht_sreq_heap(struct law_ht_sreq *req)
{
        return req->heap;
}

static sel_err_t law_ht_read_data(struct law_ht_conn *conn)
{
        struct pgc_buf *in = conn->in;
        const size_t max = pgc_buf_max(in);
        const size_t end = pgc_buf_end(in);
        const size_t pos = pgc_buf_tell(in);
        const size_t rdy = end - pos;
        const size_t avl = max - rdy;

        SEL_ASSERT(pos <= end);
        SEL_ASSERT(rdy <= max);

        if(!avl) {
                return LAW_ERR_OOB;
        } 

        enum pgc_err err = pgc_buf_read(in, conn->socket, avl);

        if(err == PGC_ERR_SYS) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        return LAW_ERR_AGAIN;
                } else {
                        return LAW_ERR_SYS;
                }
        } else {
                return err;
        }
}

static sel_err_t law_ht_read_head(
        struct law_ht_conn *conn,
        struct pgc_par *parser,
        struct pgc_buf *heap,
        size_t *scan,
        struct pgc_ast_lst **head)
{
        const char *CRLF2 = "\r\n\r\n";
        const size_t CRLF2_LEN = 4;
        const size_t CRLF2_STEP = CRLF2_LEN - 1;

        int socket = conn->socket;
        struct pgc_buf *in = conn->in;
        const size_t base = pgc_buf_tell(in);

        /* Perform an eager read. */

        const sel_err_t read_err = law_ht_read_data(conn); 

        /* Scan for the double CRLF. */

        enum pgc_err err = pgc_buf_seek(in, *scan);

        SEL_ASSERT(err == PGC_ERR_OK);

        err = pgc_buf_scan(in, CRLF2, CRLF2_LEN);

        if(err != PGC_ERR_OK) {
                
                /* The double CRLF was not encountered. */

                const size_t end = pgc_buf_end(in);
                *scan = end > CRLF2_STEP ? end - CRLF2_STEP : 0;
                err = pgc_buf_seek(in, base);

                SEL_ASSERT(err == PGC_ERR_OK);
                SEL_ASSERT(base <= end);

                if(end - base >= pgc_buf_max(in)) {
                        return LAW_ERR_OOB;
                } else if(read_err != LAW_ERR_OK) {
                        return read_err;
                } else {
                        return LAW_ERR_AGAIN;
                }   
        }
        
        /* Mark the spot where the double CRLF was found. */

        const size_t head_end = pgc_buf_tell(in);
        *scan = head_end;
        
        /* Parse the HTTP message's head. */

        err = pgc_buf_seek(in, base);

        SEL_ASSERT(err == PGC_ERR_OK);
        SEL_ASSERT(base <= head_end);

        struct pgc_buf lens; 
        pgc_buf_lens(&lens, in, head_end - base);

        err = pgc_lang_parse(parser, &lens, heap, head);
        if(err != PGC_ERR_OK) {
                return LAW_ERR_SYN;
        }

        return LAW_ERR_OK;
}


static enum law_ht_method law_ht_get_method(const char *str)
{
        if(!strcmp(str, "GET")) {
                return LAW_HT_GET;
        } else if(!strcmp(str, "POST")) {
                return LAW_HT_POST;
        } else if(!strcmp(str, "HEAD")) {
                return LAW_HT_HEAD;
        } else if(!strcmp(str, "PUT")) {
                return LAW_HT_PUT;
        } else if(!strcmp(str, "DELETE")) {
                return LAW_HT_DELETE;
        } else if(!strcmp(str, "PATCH")) {
                return LAW_HT_PATCH;
        } else if(!strcmp(str, "OPTIONS")) {
                return LAW_HT_OPTIONS;
        } else if(!strcmp(str, "TRACE")) {
                return LAW_HT_TRACE;
        } else if(!strcmp(str, "CONNECT")) {
                return LAW_HT_CONNECT;
        } else {
                return -1;
        }
}

static enum law_ht_version law_ht_get_version(const char *str)
{
        if(!strcmp(str, "HTTP/1.1")) {
                return LAW_HT_1_1;
        } else if(!strcmp(str, "HTTP/2")) {
                return LAW_HT_2;
        } else {
                return -1;
        }
}

sel_err_t law_ht_sreq_read_head(
        struct law_ht_sreq *request,
        enum law_ht_method *method,
        enum law_ht_version *version,
        struct law_uri *target,
        struct law_ht_hdrs **headers)
{
        struct pgc_stk *heap = request->heap;
        struct law_http_parsers *dict = law_http_parsers_link();
        struct pgc_par *head_par = law_http_parsers_request_head(dict);

        struct pgc_ast_lst *head;

        SEL_TRY_QUIETLY(law_ht_read_head(
                &request->conn, 
                head_par, 
                request->heap,
                &request->scan,
                &head));

        const enum law_ht_method method_d = law_ht_get_method(
                pgc_ast_tostr(pgc_ast_at(head, 0)->val));
        if(method_d == -1) {
                return LAW_ERR_METH;
        }

        const enum law_ht_version version_d = law_ht_get_version(
                pgc_ast_tostr(pgc_ast_at(head, 2)->val));
        if(version_d == -1) {
                return LAW_ERR_VERS;
        }

        *headers = pgc_stk_push(heap, sizeof(struct law_ht_hdrs));
        if(!(*headers)) {
                return LAW_ERR_OOM;
        }
        (*headers)->list = pgc_ast_at(head, 3);

        law_uri_from_ast(target, pgc_ast_tolst(pgc_ast_at(head, 1)->val));
        
        *method = method_d;
        *version = version_d;

        return LAW_ERR_OK;
}

sel_err_t law_ht_sreq_read_data(struct law_ht_sreq *request)
{
        return law_ht_read_data(&request->conn);
}

sel_err_t law_ht_sreq_write_head(
        struct law_ht_sreq *request,
        const int status,
        const size_t header_count,
        const char *headers[][2])
{       
        struct pgc_buf *out = request->conn.out;
        SEL_TRY_QUIETLY(pgc_buf_printf(out, 
                "HTTP/1.1 %i %s\r\n", 
                status,
                law_ht_status_str(status)));
        for(size_t i = 0; i < header_count; ++i) {
                SEL_TRY_QUIETLY(pgc_buf_printf(out,
                        "%s:%s\r\n",
                        headers[i][0],
                        headers[i][1]));
        }
        return pgc_buf_put(out, "\r\n", 2);
}

static sel_err_t law_ht_write_data(struct law_ht_conn *conn)
{
        struct pgc_buf *out = conn->out;
        const size_t base = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);
        const size_t rdy = end - base;

        SEL_ASSERT(base <= end);

        if(!rdy) {
                return LAW_ERR_OK;
        }

        enum pgc_err err = pgc_buf_write(out, conn->socket, rdy);

        if(err == PGC_ERR_SYS) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        return LAW_ERR_AGAIN;
                } else {
                        return LAW_ERR_SYS;
                }
        } else {
                return err;
        }
}

sel_err_t law_ht_sreq_write_data(struct law_ht_sreq *request)
{
        return law_ht_write_data(&request->conn);
}

sel_err_t law_ht_sreq_done(struct law_ht_sreq *request)
{
        return LAW_ERR_OK;
}

/* CLIENT REQUEST SECTION ###################################################*/

struct law_ht_creq *law_ht_creq_create(struct law_ht_creq_cfg *cfg)
{
        struct law_ht_creq *request = malloc(sizeof(struct law_ht_creq));
        
        const size_t inlen = cfg->in_length;
        const size_t ingrd = cfg->in_guard;
        const size_t outlen = cfg->out_length;
        const size_t outgrd = cfg->out_guard;
        const size_t heaplen = cfg->heap_length;
        const size_t heapgrd = cfg->heap_guard;

        struct law_smem *in_mem = law_smem_create(inlen, ingrd);
        struct law_smem *out_mem = law_smem_create(outlen, outgrd);
        struct law_smem *heap_mem = law_smem_create(heaplen, heapgrd);

        struct pgc_stk *heap = malloc(sizeof(struct pgc_stk));
        pgc_stk_init(heap, heaplen, law_smem_address(heap_mem));

        struct pgc_buf *in = malloc(sizeof(struct pgc_buf));
        struct pgc_buf *out = malloc(sizeof(struct pgc_buf));

        pgc_buf_init(in, law_smem_address(in), inlen, 0);
        pgc_buf_init(out, law_smem_address(out), outlen, 0);

        request->heap_mem = heap_mem;
        request->heap = heap;
        request->conn.in = in;
        request->in_mem = in_mem;
        request->conn.out = out;
        request->out_mem = out_mem;
        request->conn.socket = 0;
        request->conn.ssl = NULL;

        return request;
}

void law_ht_creq_destroy(struct law_ht_creq *request)
{
        free(request->conn.out);
        free(request->conn.in);
        free(request->heap);

        law_smem_destroy(request->out_mem);
        law_smem_destroy(request->in_mem);
        law_smem_destroy(request->heap_mem);

        free(request);
}

struct pgc_buf *law_ht_creq_out(struct law_ht_creq *request)
{
        return request->conn.out;
}

struct pgc_buf *law_ht_creq_in(struct law_ht_creq *request)
{
        return request->conn.in;
}

struct pgc_stk *law_ht_creq_heap(struct law_ht_creq *request)
{
        return request->heap;
}

sel_err_t law_ht_creq_connect(
        struct law_ht_creq *request, 
        const char *host,
        const char *service)
{
        struct addrinfo hints, *info, *i;
        int fd;

        memset(&hints, 0, sizeof(sizeof(struct addrinfo)));
        hints.ai_family = AF_UNSPEC; 
        hints.ai_socktype = SOCK_STREAM;

        if(getaddrinfo(host, service, &hints, &info)) {
                return LAW_ERR_GAI;
        }

        for(i = info; i; i = i->ai_next) {

                fd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
                if(fd < 0) {
                        continue;
                }

                const int flags = fcntl(fd, F_GETFL, 0);
                if(fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
                        close(fd);
                        continue;
                }

                if(connect(fd, i->ai_addr, i->ai_addrlen) < 0) {
                        if(errno != EINPROGRESS) {
                                close(fd);
                                continue;
                        }
                }
        }

        freeaddrinfo(info);

        if(!i) {
                return LAW_ERR_SYS;
        }

        request->conn.socket = fd;

        return LAW_ERR_OK;
}

static const char *law_ht_method_str(enum law_ht_method meth)
{
        switch(meth) {
                case LAW_HT_GET:        return "GET";
                case LAW_HT_POST:       return "POST";
                case LAW_HT_HEAD:       return "HEAD";
                case LAW_HT_PUT:        return "PUT";
                case LAW_HT_DELETE:     return "DELETE";
                case LAW_HT_PATCH:      return "PATCH";
                case LAW_HT_OPTIONS:    return "OPTIONS";
                case LAW_HT_TRACE:      return "TRACE";
                case LAW_HT_CONNECT:    return "CONNECT";
                default: SEL_ABORT();
        }
}

sel_err_t law_ht_creq_write_head(
        struct law_ht_creq *request,
        enum law_ht_method method,
        const char *target,
        const size_t hcount,
        const char *headers[][2])
{
        struct pgc_buf *buf = request->conn.out;
        SEL_TRY_QUIETLY(pgc_buf_printf(buf, 
                "%s %s HTTP/1.1\r\n",
                law_ht_method_str(method),
                target));
        for(size_t n = 0; n < hcount; ++n) {
                SEL_TRY_QUIETLY(pgc_buf_printf(buf,
                "%s: %s\r\n",
                headers[n][0],
                headers[n][1]));
        }
        SEL_TRY_QUIETLY(pgc_buf_put(buf, "\r\n", 2));
        return LAW_ERR_OK;
}

ssize_t law_ht_creq_write_data(struct law_ht_creq *request)
{
        return law_ht_write_data(&request->conn);
}

/* status-line = HTTP-version SP status-code SP reason-phrase CRLF */

sel_err_t law_ht_creq_read_head(
        struct law_ht_creq *request,
        enum law_ht_version *version,
        int *status,
        struct law_ht_hdrs **headers)
{
        struct pgc_stk *heap = request->heap;
        struct law_http_parsers *dict = law_http_parsers_link();
        struct pgc_par *head_par = law_http_parsers_response_head(dict);

        struct pgc_ast_lst *head;

        SEL_TRY_QUIETLY(law_ht_read_head(
                &request->conn, 
                head_par, 
                request->heap,
                &request->scan,
                &head));

        const enum law_ht_version version_d = law_ht_get_version(
                pgc_ast_tostr(pgc_ast_at(head, 0)->val));
        if(version_d == -1) {
                return LAW_ERR_VERS;
        }

        const int status_d = (int)pgc_ast_touint32(pgc_ast_at(head, 1)->val);

        *headers = pgc_stk_push(heap, sizeof(struct law_ht_hdrs));
        if(!(*headers)) {
                return LAW_ERR_OOM;
        }
        (*headers)->list = pgc_ast_at(head, 2);

        *version = version_d;
        *status = status_d;

        return LAW_ERR_OK;
}

ssize_t law_ht_creq_read_data(struct law_ht_creq *request)
{
        return law_ht_read_data(&(request->conn));
}

sel_err_t law_ht_creq_done(struct law_ht_creq *request)
{
        return LAW_ERR_OK;
}

/* SERVER CONTEXT SECTION ###################################################*/

struct law_ht_sctx *law_ht_sctx_create(struct law_ht_sctx_cfg *cfg)
{
        struct law_ht_sctx *http = malloc(sizeof(struct law_ht_sctx));
        http->cfg = cfg;
        return http;
}

void law_ht_sctx_destroy(struct law_ht_sctx *http)
{
        free(http);
}

sel_err_t law_ht_accept(
        struct law_srv *server,
        int socket,
        void *state)
{
        struct law_ht_sctx *http = state;
        struct law_ht_sctx_cfg *cfg = http->cfg;

        const size_t in_length = cfg->in_length;
        const size_t in_guard = cfg->in_guard;
        const size_t out_length = cfg->out_length;
        const size_t out_guard = cfg->out_guard;
        const size_t heap_length = cfg->heap_length;
        const size_t heap_guard = cfg->heap_guard;

        struct law_smem *in_mem = law_smem_create(in_length, in_guard);
        struct law_smem *out_mem = law_smem_create(out_length, out_guard);
        struct law_smem *heap_mem = law_smem_create(heap_length, heap_guard);

        struct pgc_stk heap;
        pgc_stk_init(&heap, heap_length, law_smem_address(heap_mem));

        struct pgc_buf in, out;
        pgc_buf_init(&in, law_smem_address(in_mem), in_length, 0);
        pgc_buf_init(&out, law_smem_address(out_mem), out_length, 0);

        struct law_ht_sreq request;
        request.conn.socket = socket;
        request.conn.in = &in;
        request.conn.out = &out;
        request.heap = &heap;
        request.conn.ssl = NULL;
        request.scan = 0;

        sel_err_t error = http->cfg->handler(server, http, &request);
        
        law_smem_destroy(heap_mem);
        law_smem_destroy(out_mem);
        law_smem_destroy(in_mem);

        return error;
}       

const char *law_ht_status_str(const int code)
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

/* PARSER SECTION ###########################################################*/

enum pgc_err law_http_cap_method(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_METHOD, pgc_lang_readstr);
}

enum pgc_err law_http_cap_version(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_VERSION, pgc_lang_readstr);
}

enum pgc_err law_http_cap_field_name(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_FIELD_NAME, pgc_lang_readstr);
}

enum pgc_err law_http_cap_field_value(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_FIELD_VALUE, pgc_lang_readstr);
}

enum pgc_err law_http_cap_field(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buffer, state, arg, LAW_HT_FIELD, 0);
}

enum pgc_err law_http_cap_origin_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HT_ORIGIN_FORM, 0);
}

enum pgc_err law_http_cap_absolute_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HT_ABSOLUTE_FORM, 0);
}

enum pgc_err law_http_cap_authority_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HT_AUTHORITY_FORM, 0);
}

struct law_http_parsers *law_http_parsers_link()
{
        /*      dec cap_absolute_URI;
                dec cap_origin_URI;
                dec cap_authority; */

        static struct law_http_parsers *https = NULL;
        static struct law_uri_parsers *urips = NULL;

        if(https) {
                return https;
        }

        https = export_law_http_parsers();
        urips = export_law_uri_parsers();

        struct pgc_par *abs_URI = law_uri_parsers_cap_absolute_URI(urips);
        struct pgc_par *ori_URI = law_uri_parsers_cap_origin_URI(urips);
        struct pgc_par *cap_auth = law_uri_parsers_cap_authority(urips);

        struct pgc_par *abs_URI_l = law_http_parsers_cap_absolute_URI(https);
        struct pgc_par *ori_URI_l = law_http_parsers_cap_origin_URI(https);
        struct pgc_par *cap_auth_l = law_http_parsers_cap_authority(https);

        abs_URI_l->u.lnk = abs_URI;
        ori_URI_l->u.lnk = ori_URI;
        cap_auth_l->u.lnk = cap_auth;

        return https;
}
