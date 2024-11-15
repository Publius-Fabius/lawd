
#include "lawd/http/server.h"
#include "lawd/safemem.h"
#include <errno.h>
#include <openssl/err.h>

extern const struct pgc_par law_htp_request_head;

sel_err_t law_hts_read_head(
        struct law_hts_req *req,
        struct law_hts_head *head)
{
        struct pgc_stk *heap = req->heap;
        const struct pgc_par *head_par = &law_htp_request_head;

        struct pgc_ast_lst *list = NULL;

        SEL_TRY_QUIETLY(law_htc_read_head(
                &req->conn, 
                head_par, 
                req->stack,
                heap,
                &list));

        head->method = pgc_ast_tostr(pgc_ast_at(list, 0)->val);
        law_uri_from_ast(
                &head->target, 
                pgc_ast_tolst(pgc_ast_at(list, 1)->val));
        head->version = pgc_ast_tostr(pgc_ast_at(list, 2)->val);

        head->headers = law_hth_from_ast(
                pgc_ast_at(list, 3), 
                (void *(*)(void*,const size_t))pgc_stk_push, 
                heap);
        if(!head->headers) 
                return LAW_ERR_OOM;

        return LAW_ERR_OK;
}

const char *law_hts_status_str(const int status_code)
{
        SEL_ASSERT(status_code > 0);
        switch(status_code) {

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

                default: return "Unknown Status Code";
        }
}

sel_err_t law_hts_set_status(
        struct law_hts_req *req,
        const char *version,
        const int status,
        const char *reason)
{
        return pgc_buf_printf(
                req->conn.out, 
                "%s %i %s\r\n", 
                version,
                status,
                reason);
}

sel_err_t law_hts_add_header(
        struct law_hts_req *req,
        const char *name,
        const char *value)
{
        return pgc_buf_printf(
                req->conn.out,
                "%s: %s\r\n",
                name,
                value);
}

sel_err_t law_hts_begin_body(struct law_hts_req *req)
{
        return pgc_buf_put(req->conn.out, "\r\n", 2);
}

struct law_hts_cfg law_hts_sanity()
{
        struct law_hts_cfg cfg;
        memset(&cfg, 0, sizeof(struct law_hts_cfg));
        cfg.in_length           = 0x2000;
        cfg.in_guard            = 0x1000;
        cfg.out_length          = 0x2000;
        cfg.out_guard           = 0x1000;
        cfg.stack_length        = 0x1000;
        cfg.stack_guard         = 0x1000;
        cfg.heap_length         = 0xF000;
        cfg.heap_guard          = 0x1000;
        cfg.security            = LAW_HTC_UNSECURED;
        return cfg;
}

struct law_hts_ctx *law_hts_create(struct law_hts_cfg *cfg)
{
        struct law_hts_ctx *ctx = calloc(1, sizeof(struct law_hts_ctx));
        ctx->cfg = *cfg;
        ctx->ssl_ctx = NULL;
        return ctx;
}

void law_hts_destroy(struct law_hts_ctx *ctx)
{
        free(ctx);
}

sel_err_t law_hts_init_ssl_ctx(struct law_hts_ctx *ctx)
{
        ERR_clear_error();
        SSL_CTX *context = SSL_CTX_new(TLS_server_method());
        if(!context) 
                return LAW_ERR_SSL;

        ERR_clear_error();
        int err = SSL_CTX_use_certificate_file(
                context, 
                ctx->cfg.cert, 
                SSL_FILETYPE_PEM);
        if(err < 0) {
                SSL_CTX_free(context);
                return LAW_ERR_SSL;
        }

        ERR_clear_error();
        err = SSL_CTX_use_PrivateKey_file(
                context, 
                ctx->cfg.pkey, 
                SSL_FILETYPE_PEM);
        if(err < 0) {
                SSL_CTX_free(context);
                return LAW_ERR_SSL;
        }

        ctx->ssl_ctx = context;

        return LAW_ERR_OK;
}

sel_err_t law_hts_accept(
        struct law_worker *worker,
        int socket,
        struct law_data data)
{
        struct law_hts_ctx *context = data.u.ptr;
        struct law_hts_cfg *cfg = &context->cfg;

        const size_t in_length = cfg->in_length;
        const size_t in_guard = cfg->in_guard;
        const size_t out_length = cfg->out_length;
        const size_t out_guard = cfg->out_guard;
        const size_t heap_length = cfg->heap_length;
        const size_t heap_guard = cfg->heap_guard;
        const size_t stack_length = cfg->stack_length;
        const size_t stack_guard = cfg->stack_guard;

        struct law_smem *in_mem = law_smem_create(in_length, in_guard);
        if(!in_mem) goto OOM;
        struct law_smem *out_mem = law_smem_create(out_length, out_guard);
        if(!out_mem) goto DESTROY_IN;
        struct law_smem *heap_mem = law_smem_create(heap_length, heap_guard);
        if(!heap_mem) goto DESTROY_OUT;
        struct law_smem *stk_mem = law_smem_create(stack_length, stack_guard);
        if(!stk_mem) goto DESTROY_HEAP;

        struct pgc_stk heap, stack;
        pgc_stk_init(&heap, law_smem_address(heap_mem), heap_length);
        pgc_stk_init(&stack, law_smem_address(stk_mem), stack_length);

        struct pgc_buf in, out;
        pgc_buf_init(&in, law_smem_address(in_mem), in_length, 0);
        pgc_buf_init(&out, law_smem_address(out_mem), out_length, 0);

        struct law_hts_req request;
        request.conn.socket = socket;
        request.conn.in = &in;
        request.conn.out = &out;
        request.heap = &heap;
        request.stack = &stack;
        request.conn.ssl = NULL;
        request.conn.security = LAW_HTC_UNSECURED;
        request.ctx = context;

        sel_err_t error = context->cfg.callback(
                worker, 
                &request, 
                context->cfg.data);

        law_smem_destroy(stk_mem);
        law_smem_destroy(heap_mem);
        law_smem_destroy(out_mem);
        law_smem_destroy(in_mem);

        return error;

        DESTROY_HEAP:
        law_smem_destroy(heap_mem);
        DESTROY_OUT:
        law_smem_destroy(out_mem);
        DESTROY_IN:
        law_smem_destroy(in_mem);
        OOM:
        return LAW_ERR_OOM;
}
