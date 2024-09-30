#define _POSIX_C_SOURCE 200112L

#include "lawd/http.h"
#include "lawd/safemem.h"
#include "pgenc/lang.h"
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
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
        enum law_ht_security security;
        SSL *ssl;
        struct pgc_buf *in;
        struct pgc_buf *out;
};

struct law_ht_sreq {
        struct law_ht_conn conn;
        struct pgc_stk *heap;
        struct law_ht_sctx *context;
};

struct law_ht_creq {
        struct law_ht_conn conn;
        struct pgc_stk *heap;
        struct law_smem *in_mem;
        struct law_smem *out_mem;
        struct law_smem *heap_mem;  
};

struct law_ht_sctx {
        struct law_ht_sctx_cfg *cfg;
        SSL_CTX *ssl_context;
};

/* HEADERS SECTION ##########################################################*/

const char *law_ht_hdrs_get(
        struct law_ht_hdrs *hdrs,
        const char *name)
{
        for(struct pgc_ast_lst *l = hdrs->list; l; l = l->nxt) {
                struct pgc_ast_lst *tuple = pgc_ast_tolst(l->val);
                if(strcmp(pgc_ast_tostr(tuple->val), name)) {
                        continue;
                } else if(tuple->nxt) {
                        return pgc_ast_tostr(tuple->nxt->val);
                } else {
                        return "";
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
        free(iter);
}

struct law_ht_hdrs_i *law_ht_hdrs_i_next(
        struct law_ht_hdrs_i *iter,
        const char **name,
        const char **value)
{
        if(!iter->list) {
                law_ht_hdrs_i_free(iter);
                return NULL;
        }
        struct pgc_ast_lst *ele = pgc_ast_tolst(iter->list->val);
        *name = pgc_ast_tostr(ele->val);
        *value = ele->nxt ? pgc_ast_tostr(ele->nxt->val) : "";
        iter->list = iter->list->nxt;
        return iter;
}

/* SERVER REQUEST SECTION ###################################################*/

int law_ht_sreq_socket(struct law_ht_sreq *req)
{
        return req->conn.socket;
}

SSL *law_ht_sreq_ssl(struct law_ht_sreq *request)
{
        return request->conn.ssl;
}

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

sel_err_t law_ht_sreq_ssl_accept(struct law_ht_sreq *request)
{
        ERR_clear_error();
        SSL *ssl = SSL_new(request->context->ssl_context);
        if(!ssl) {
                return LAW_ERR_SSL;
        }

        ERR_clear_error();
        int ssl_err = SSL_set_fd(ssl, request->conn.socket);
        if(ssl_err == 0) {
                SSL_free(ssl);
                return LAW_ERR_SSL;
        }

        request->conn.security = LAW_HT_SSL;
        request->conn.ssl = ssl;

        ERR_clear_error();
        ssl_err = SSL_accept(ssl);

        if(ssl_err == 0) {
                /* SSL was shut down gracefully. */
                SSL_free(ssl);
                return LAW_ERR_EOF;
        } else if(ssl_err < 0) {
                /* An error or polling data. */
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ:
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE:
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL:
                                SSL_free(ssl);
                                return LAW_ERR_SYS;
                        default:
                                SSL_free(ssl);
                                return LAW_ERR_SSL;
                }
        } else {
                /* Accept totally completed. */
                return LAW_ERR_OK;
        }
}

sel_err_t law_ht_sreq_ssl_shutdown(struct law_ht_sreq *request)
{
        SSL *ssl = request->conn.ssl;

        ERR_clear_error();
        int ssl_err = SSL_shutdown(ssl);

        if(ssl_err == 0) {
                /* Notify sent, waiting on client response. */
                return LAW_ERR_WNTR;
        } else if(ssl_err < 0) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ:
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE:
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL:
                                SSL_free(ssl);
                                return LAW_ERR_SYS;
                        default:
                                SSL_free(ssl);
                                return LAW_ERR_SSL;
                }
        } else {
                /* Shutdown was a success. */
                SSL_free(ssl);
                return LAW_ERR_OK; 
        }
}

static sel_err_t law_ht_read_SSL(
        struct pgc_buf *in,
        SSL *ssl,
        const size_t nbytes)
{
        int ssl_err;
        
        ERR_clear_error();
        const enum pgc_err err = pgc_buf_sread(in, ssl, nbytes, &ssl_err);

        SEL_ASSERT(err != PGC_ERR_OOB);

        if(err == PGC_ERR_SSL) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE:
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL:
                                return LAW_ERR_SYS;
                        default:
                                return LAW_ERR_SSL;
                }
        } else {
                return err;
        }
}

static sel_err_t law_ht_read_sys(
        struct pgc_buf *in,
        int socket,
        const size_t nbytes)
{
        const enum pgc_err err = pgc_buf_read(in, socket, nbytes);

        SEL_ASSERT(err != PGC_ERR_OOB);

        if(err == PGC_ERR_SYS) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        return LAW_ERR_WNTR;
                } else {
                        return LAW_ERR_SYS;
                }
        } else {
                return err;
        }
}

static sel_err_t law_ht_read_data(struct law_ht_conn *conn)
{
        struct pgc_buf *in = conn->in;
        const size_t max = pgc_buf_max(in);
        const size_t end = pgc_buf_end(in);
        const size_t pos = pgc_buf_tell(in);

        const size_t block = end - pos;
        const size_t avail = max - block;

        SEL_ASSERT(pos <= end);
        SEL_ASSERT(block <= max);

        if(!avail) {
                return LAW_ERR_OOB;
        } else switch(conn->security) {
                case LAW_HT_UNSECURED:
                        return law_ht_read_sys(in, conn->socket, avail);
                case LAW_HT_SSL: 
                        return law_ht_read_SSL(in, conn->ssl, avail);
                default: 
                        return SEL_ABORT();
        }
}

static sel_err_t law_ht_read_head(
        struct law_ht_conn *conn,
        struct pgc_par *parser,
        struct pgc_stk *heap,
        struct pgc_ast_lst **head)
{
        static const char *CRLF2 = "\r\n\r\n";
        static const size_t CRLF2_LEN = 4;

        struct pgc_buf *in = conn->in;
        const size_t base = pgc_buf_tell(in);
        const size_t max = pgc_buf_max(in);

        const sel_err_t read_err = law_ht_read_data(conn); 

        enum pgc_err err = pgc_buf_scan(in, (void*)CRLF2, CRLF2_LEN);

        if(err != PGC_ERR_OK) {
                const size_t end = pgc_buf_end(in);
                const size_t block = end - base;
                err = pgc_buf_seek(in, base);

                SEL_ASSERT(err == PGC_ERR_OK);
                SEL_ASSERT(base <= end);
                SEL_ASSERT(block <= max)

                if(block == max) {
                        return LAW_ERR_OOB;
                } else if(read_err == LAW_ERR_OK) {
                        return LAW_ERR_WNTR;
                } else {
                        return read_err;
                }   
        }
        
        const size_t head_end = pgc_buf_tell(in);
        
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

sel_err_t law_ht_sreq_read_head(
        struct law_ht_sreq *request,
        struct law_ht_reqhead *head)
{
        struct pgc_stk *heap = request->heap;
        struct law_ht_parsers *dict = law_ht_parsers_link();
        struct pgc_par *head_par = law_ht_parsers_request_head(dict);

        struct pgc_ast_lst *list;

        SEL_TRY_QUIETLY(law_ht_read_head(
                &request->conn, 
                head_par, 
                request->heap,
                &list));

        head->method = pgc_ast_tostr(pgc_ast_at(list, 0)->val);
        law_uri_from_ast(
                &head->target, 
                pgc_ast_tolst(pgc_ast_at(list, 1)->val));
        head->version = pgc_ast_tostr(pgc_ast_at(list, 2)->val);

        head->headers = pgc_stk_push(heap, sizeof(struct law_ht_hdrs));
        if(!head->headers) {
                return LAW_ERR_OOM;
        }
        head->headers->list = pgc_ast_at(list, 3);

        return LAW_ERR_OK;
}

sel_err_t law_ht_sreq_read_data(struct law_ht_sreq *request)
{
        return law_ht_read_data(&request->conn);
}

sel_err_t law_ht_sreq_set_status(
        struct law_ht_sreq *request,
        const char *version,
        const int status,
        const char *reason)
{       
        return pgc_buf_printf(
                request->conn.out, 
                "%s %i %s\r\n", 
                version,
                status,
                reason);
}

sel_err_t law_ht_sreq_add_header(
        struct law_ht_sreq *request,
        const char *name,
        const char *value)
{
        return pgc_buf_printf(
                request->conn.out,
                "%s: %s\r\n",
                name,
                value);
}

sel_err_t law_ht_sreq_begin_body(struct law_ht_sreq *request)
{
        return pgc_buf_put(request->conn.out, "\r\n", 2);
}

static sel_err_t law_ht_write_sys(
        struct pgc_buf *out,
        int socket,
        const size_t nbytes)
{
        enum pgc_err err = pgc_buf_write(out, socket, nbytes);

        SEL_ASSERT(err != PGC_ERR_OOB);

        if(err == PGC_ERR_SYS) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                        return LAW_ERR_WNTW;
                } else {
                        return LAW_ERR_SYS;
                }
        } else {
                return err;
        }
}

static sel_err_t law_ht_write_SSL(
        struct pgc_buf *out,
        SSL *ssl,
        const size_t nbytes)
{
        int ssl_err;
        
        ERR_clear_error();
        const enum pgc_err err = pgc_buf_swrite(out, ssl, nbytes, &ssl_err);

        SEL_ASSERT(err != PGC_ERR_OOB);

        if(err == PGC_ERR_SSL) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE:
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL:
                                return LAW_ERR_SYS;
                        default:
                                return LAW_ERR_SSL;
                }
        } else {
                return err;
        }
}

static sel_err_t law_ht_write_data(struct law_ht_conn *conn)
{
        struct pgc_buf *out = conn->out;
        const size_t base = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);
        const size_t block = end - base;

        SEL_ASSERT(base <= end);

        if(!block) {
                return LAW_ERR_OK;
        } else switch(conn->security) {
                case LAW_HT_SSL:
                        return law_ht_write_SSL(out, conn->ssl, block);
                case LAW_HT_UNSECURED:
                        return law_ht_write_sys(out, conn->socket, block);
                default:
                        return SEL_ABORT();
        }
}

sel_err_t law_ht_sreq_write_data(struct law_ht_sreq *request)
{
        return law_ht_write_data(&request->conn);
}

sel_err_t law_ht_sreq_close(struct law_ht_sreq *request)
{
        close(request->conn.socket);
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

        pgc_buf_init(in, law_smem_address(in_mem), inlen, 0);
        pgc_buf_init(out, law_smem_address(out_mem), outlen, 0);

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

int law_ht_creq_socket(struct law_ht_creq *request)
{
        return request->conn.socket;
}

SSL *law_ht_creq_SSL(struct law_ht_creq *request)
{
        return request->conn.ssl;
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

sel_err_t law_ht_creq_request_line(
        struct law_ht_creq *request,
        const char *method,
        const char *target,
        const char *version)
{
        return pgc_buf_printf(
                request->conn.out, 
                "%s %s %s\r\n",
                method,
                target,
                version);
}

sel_err_t law_ht_creq_header(
        struct law_ht_creq *request,
        const char *name,
        const char *value)
{
        return pgc_buf_printf(
                request->conn.out, 
                "%s: %s\r\n",
                name,
                value);
}

sel_err_t law_ht_creq_body(struct law_ht_creq *request)
{
        return pgc_buf_put(request->conn.out, "\r\n", 2);
}

sel_err_t law_ht_creq_write_data(struct law_ht_creq *request)
{
        return law_ht_write_data(&request->conn);
}

sel_err_t law_ht_creq_send(struct law_ht_creq *request)
{
        // FINISH ME!
        return LAW_ERR_SYS;
}

/* status-line = HTTP-version SP status-code SP reason-phrase CRLF */

sel_err_t law_ht_creq_read_head(
        struct law_ht_creq *request,
        struct law_ht_reshead *head)
{
        struct pgc_stk *heap = request->heap;
        struct law_ht_parsers *dict = law_ht_parsers_link();
        struct pgc_par *head_par = law_ht_parsers_response_head(dict);

        struct pgc_ast_lst *list;

        SEL_TRY_QUIETLY(law_ht_read_head(
                &request->conn, 
                head_par, 
                request->heap,
                &list));

        head->version = pgc_ast_tostr(pgc_ast_at(list, 0)->val);
        head->status = (int)pgc_ast_touint32(pgc_ast_at(list, 1)->val);

        head->headers = pgc_stk_push(heap, sizeof(struct law_ht_hdrs));
        if(!head->headers) {
                return LAW_ERR_OOM;
        }
        head->headers->list = pgc_ast_at(list, 2);

        return LAW_ERR_OK;
}

ssize_t law_ht_creq_read_data(struct law_ht_creq *request)
{
        return law_ht_read_data(&(request->conn));
}

sel_err_t law_ht_creq_done(struct law_ht_creq *request)
{
        close(request->conn.socket);
        return LAW_ERR_OK;
}

/* SERVER CONTEXT SECTION ###################################################*/

struct law_ht_sctx *law_ht_sctx_create(struct law_ht_sctx_cfg *cfg)
{
        struct law_ht_sctx *http = malloc(sizeof(struct law_ht_sctx));
        http->cfg = cfg;
        http->ssl_context = NULL;
        return http;
}

void law_ht_sctx_destroy(struct law_ht_sctx *http)
{
        if(http->ssl_context) {
                SSL_CTX_free(http->ssl_context);
        }
        free(http);
}

sel_err_t law_ht_sctx_init(struct law_ht_sctx *http)
{
        ERR_clear_error();
        SSL_CTX *context = SSL_CTX_new(TLS_server_method());
        if(!context) {
                return LAW_ERR_SSL;
        }

        ERR_clear_error();
        int err = SSL_CTX_use_certificate_file(
                context, 
                http->cfg->certificate, 
                SSL_FILETYPE_PEM);
        if(err < 0) {
                SSL_CTX_free(context);
                return LAW_ERR_SSL;
        }

        ERR_clear_error();
        err = SSL_CTX_use_PrivateKey_file(
                context, 
                http->cfg->private_key, 
                SSL_FILETYPE_PEM);
        if(err < 0) {
                SSL_CTX_free(context);
                return LAW_ERR_SSL;
        }

        http->ssl_context = context;

        return LAW_ERR_OK;
}

sel_err_t law_ht_accept(
        struct law_server *server,
        int socket,
        struct law_data *data)
{
        struct law_ht_sctx *context = data->u.ptr;
        struct law_ht_sctx_cfg *cfg = context->cfg;

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
        request.context = context;
        request.conn.security = LAW_HT_UNSECURED;

        sel_err_t error = context->cfg->callback(
                server, 
                &request, 
                context->cfg->state);
        
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

                default: return "Unknown HTTP Error";
        }
}

/* PARSER SECTION ###########################################################*/

static enum pgc_err law_ht_read_uint32(
        struct pgc_buf *buffer,
        void *state,
        const int16_t tag)
{
        return pgc_lang_readenc(
                buffer, 
                state, 
                PGC_AST_UINT32, 
                LAW_HT_STATUS, 
                10, 
                pgc_buf_decode_dec, 
                pgc_buf_decode_uint32);
}

enum pgc_err law_ht_cap_status(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_METHOD, law_ht_read_uint32);
}

enum pgc_err law_ht_cap_method(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_METHOD, pgc_lang_readstr);
}

enum pgc_err law_ht_cap_version(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_VERSION, pgc_lang_readstr);
}

enum pgc_err law_ht_cap_field_name(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_FIELD_NAME, pgc_lang_readstr);
}

enum pgc_err law_ht_cap_field_value(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_HT_FIELD_VALUE, pgc_lang_readstr);
}

enum pgc_err law_ht_cap_field(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buffer, state, arg, LAW_HT_FIELD, 0);
}

enum pgc_err law_ht_cap_origin_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HT_ORIGIN_FORM, 0);
}

enum pgc_err law_ht_cap_absolute_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HT_ABSOLUTE_FORM, 0);
}

enum pgc_err law_ht_cap_authority_form(
        struct pgc_buf *buf,
        void *st,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(buf, st, arg, LAW_HT_AUTHORITY_FORM, 0);
}

struct law_ht_parsers *law_ht_parsers_link()
{
        /*      dec cap_absolute_URI;
                dec cap_origin_URI;
                dec cap_authority; */

        static struct law_ht_parsers *https = NULL;
        static struct law_uri_parsers *urips = NULL;

        if(https) {
                return https;
        }

        https = export_law_ht_parsers();
        urips = export_law_uri_parsers();

        struct pgc_par *abs_URI = law_uri_parsers_cap_absolute_URI(urips);
        struct pgc_par *ori_URI = law_uri_parsers_cap_origin_URI(urips);
        struct pgc_par *cap_auth = law_uri_parsers_cap_authority(urips);

        struct pgc_par *abs_URI_l = law_ht_parsers_cap_absolute_URI(https);
        struct pgc_par *ori_URI_l = law_ht_parsers_cap_origin_URI(https);
        struct pgc_par *cap_auth_l = law_ht_parsers_cap_authority(https);

        abs_URI_l->u.lnk = abs_URI;
        ori_URI_l->u.lnk = ori_URI;
        cap_auth_l->u.lnk = cap_auth;

        return https;
}
