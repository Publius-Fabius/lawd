
#include "lawd/private/http_server.h"
#include "lawd/private/http_headers.h"
#include "pubmt/linked_list.h"
#include "pgenc/lang.h"
#include <errno.h>
#include <openssl/err.h>
#include <unistd.h>

extern const struct pgc_par law_htp_request_line;

extern const struct pgc_par law_htp_headers;

law_htconn_t *law_hts_get_conn(law_hts_req_t *request)
{
        return &request->conn;
}

struct pgc_stk *law_hts_get_stack(law_hts_req_t *request)
{
        return request->stack;
}

struct pgc_stk *law_hts_get_heap(law_hts_req_t *request)
{
        return request->heap;
}

int law_hts_get_security(law_htserver_t *server)
{
        return server->cfg.security;
}

typedef struct law_hts_read_args {
        law_hts_req_t *req;
        void *ptr;
        size_t base;
} law_hts_read_args_t;

static sel_err_t law_hts_read_scan_parse(
        law_hts_req_t *req,
        const size_t base,
        void *delim,
        const size_t dlen,
        const struct pgc_par *parser,
        struct pgc_ast_lst **list)
{
        SEL_ASSERT(req);

        struct pgc_buf *in = req->conn.in;
        struct pgc_stk 
                *stack = req->stack,
                *heap = req->heap;
        const size_t max = pgc_buf_max(in);

        SEL_ASSERT(in && stack && heap && base <= pgc_buf_end(in));

        (void)law_err_clear();
        
        if(pgc_buf_end(in) - base > max) 
                return LAW_ERR_PUSH(LAW_ERR_OOB, "early_out_check");

        sel_err_t err = law_htc_read_scan(&req->conn, base, delim, dlen);

        switch(err) {
                case LAW_ERR_OK:
                        break;
                case LAW_ERR_WANTR:
                case LAW_ERR_WANTW:
                        return err;
                default:
                        return LAW_ERR_PUSH(err, "law_htc_read_scan");
        }
        
        const size_t end = pgc_buf_tell(in);

        SEL_TEST(pgc_buf_seek(in, base) == LAW_ERR_OK);
  
        struct pgc_buf lens; 
        (void)pgc_buf_lens(&lens, in, end - base);

        switch(pgc_lang_parse_ex(
                parser, 
                stack, 
                &lens, 
                heap, 
                list)) 
        {
                case LAW_ERR_OK:
                        break;
                case LAW_ERR_OOM: 
                        return LAW_ERR_PUSH(LAW_ERR_OOM, "pgc_lang_parse_ex");
                default: 
                        return LAW_ERR_PUSH(LAW_ERR_SYN, "pgc_lang_parse_ex");
        }

        if(pgc_buf_tell(&lens) != pgc_buf_end(&lens)) 
                return LAW_ERR_PUSH(LAW_ERR_SYN, "parse_incomplete");

        SEL_TEST(pgc_buf_seek(in, end) == LAW_ERR_OK);

        return LAW_ERR_OK;
}

sel_err_t law_hts_read_reqline(
        law_hts_req_t *req,
        law_hts_reqline_t *reqline,
        const size_t base)
{
        static char *CRLF = "\r\n";

        SEL_ASSERT(reqline);

        struct pgc_ast_lst *list = NULL;

        sel_err_t err = law_hts_read_scan_parse(
                req,
                base,
                CRLF,
                2,
                &law_htp_request_line,
                &list);
        if(err != LAW_ERR_OK) return err;

        (void)memset(reqline, 0, sizeof(law_hts_reqline_t));

        reqline->method = pgc_ast_tostr(pgc_ast_at(list, 0)->val);

        (void)law_uri_from_ast(
                &reqline->target, 
                pgc_ast_tolst(pgc_ast_at(list, 1)->val));

        reqline->version = pgc_ast_tostr(pgc_ast_at(list, 2)->val);

        return LAW_ERR_OK;
}

sel_err_t law_hts_read_reqline_cb(int fd, void *state)
{
        law_hts_read_args_t *args = state;
        return law_hts_read_reqline(args->req, args->ptr, args->base);
}

sel_err_t law_hts_read_reqline_sync(
        law_worker_t *worker, 
        law_time_t timeout,
        law_hts_req_t *request,
        law_hts_reqline_t *reqline,
        const size_t base)
{
        law_hts_read_args_t args = { 
                .req = request, 
                .ptr = reqline,
                .base = base };
        return law_sync(
                worker, 
                timeout, 
                law_hts_read_reqline_cb, 
                request->conn.socket, 
                &args);
}

sel_err_t law_hts_read_headers(
        law_hts_req_t *req,
        law_htheaders_t *headers,
        const size_t base)
{
        static char *CRLF2 = "\r\n\r\n";

        SEL_ASSERT(headers);

        (void)memset(headers, 0, sizeof(law_htheaders_t));

        sel_err_t err = law_hts_read_scan_parse(
                req,
                base,
                CRLF2,
                4,
                &law_htp_headers,
                &headers->list);
        if(err != LAW_ERR_OK) return err;

        return LAW_ERR_OK;
}

sel_err_t law_hts_read_headers_cb(int fd, void *state)
{
        law_hts_read_args_t *args = state;
        return law_hts_read_headers(args->req, args->ptr, args->base);
}

sel_err_t law_hts_read_headers_sync(
        law_worker_t *worker, 
        law_time_t timeout,
        law_hts_req_t *request,
        law_htheaders_t *headers,
        const size_t base)
{
        law_hts_read_args_t args = { 
                .req = request, 
                .ptr = headers,
                .base = base };
        return law_sync(
                worker, 
                timeout, 
                law_hts_read_headers_cb, 
                request->conn.socket, 
                &args);
}

sel_err_t law_hts_set_status(
        law_hts_req_t *req,
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
        law_hts_req_t *req,
        const char *name,
        const char *value)
{
        return pgc_buf_printf(
                req->conn.out,
                "%s: %s\r\n",
                name,
                value);
}

sel_err_t law_hts_begin_body(law_hts_req_t *req)
{
        return pgc_buf_put(req->conn.out, "\r\n", 2);
}

law_htserver_cfg_t law_htserver_sanity()
{
        law_htserver_cfg_t cfg;
        memset(&cfg, 0, sizeof(law_htserver_cfg_t));
        cfg.in                  = 0x2000;
        cfg.out                 = 0x2000;
        cfg.stack               = 0x1000;
        cfg.heap                = 0xF000;
        cfg.security            = LAW_HTC_UNSECURED;
        return cfg;
}

law_hts_buf_t *law_hts_buf_create(const size_t length)
{
        law_hts_buf_t *buf = calloc(1, sizeof(law_hts_buf_t));
        if(!buf) return NULL;

        buf->mapping = law_smem_create(length, 4096);
        if(!buf->mapping) {
                free(buf);
                return NULL;
        }

        pgc_buf_init(
                &buf->buffer, 
                law_smem_address(buf->mapping), 
                length, 
                0);

        return buf;
}

void law_hts_buf_destroy(law_hts_buf_t *buf)
{
        if(!buf) return;
        law_smem_destroy(buf->mapping);
        free(buf);
}

law_hts_stk_t *law_hts_stk_create(const size_t length)
{
        law_hts_stk_t *stk = calloc(1, sizeof(law_hts_stk_t));
        if(!stk) return NULL;

        stk->mapping = law_smem_create(length, 4096);
        if(!stk->mapping) {
                free(stk);
                return NULL;
        }

        pgc_stk_init(
                &stk->stack, 
                law_smem_address(stk->mapping), 
                length);

        return stk;
}

void law_hts_stk_destroy(law_hts_stk_t *stk)
{
        if(!stk) return;
        law_smem_destroy(stk->mapping);
        free(stk);
}

void *law_hts_stk_get_next(void *node)
{
        return ((law_hts_stk_t*)node)->next;
}

void law_hts_stk_set_next(void *node, void *next)
{
        ((law_hts_stk_t*)node)->next = next;
}

void *law_hts_buf_get_next(void *node)
{
        return ((law_hts_buf_t*)node)->next;
}

void law_hts_buf_set_next(void *node, void *next)
{
        ((law_hts_buf_t*)node)->next = next;
}

pmt_ll_node_iface_t law_hts_buf_iface = {
        .get_next = law_hts_buf_get_next,
        .set_next = law_hts_buf_set_next
};

pmt_ll_node_iface_t law_hts_stk_iface = {
        .get_next = law_hts_stk_get_next,
        .set_next = law_hts_stk_set_next
};

void law_hts_buf_pool_free(law_hts_pool_t *pool)
{
        law_hts_buf_t *buf = NULL;
        while((buf = pmt_ll_node_remove_first(
                &law_hts_buf_iface, 
                (void**)&pool->list))) 
        {
                law_hts_buf_destroy(buf);
        }
}

law_hts_pool_t *law_hts_buf_pool_init(
        law_hts_pool_t *pool,
        const size_t pool_size, 
        const size_t buf_length)
{
        memset(pool, 0, sizeof(law_hts_pool_t));

        law_hts_buf_t *buf = NULL;
        
        for(int n = 0; n < pool_size; ++n) {
                if(!(buf = law_hts_buf_create(buf_length))) {
                        law_hts_buf_pool_free(pool);
                        return NULL;
                }
                pool->list = pmt_ll_node_push_front(
                        &law_hts_buf_iface, 
                        pool->list, 
                        buf);
        }

        return pool;
}

law_hts_buf_t *law_hts_buf_pool_pop(law_hts_pool_t *pool)
{
        return pmt_ll_node_remove_first(
                &law_hts_buf_iface, 
                &pool->list);
}

void law_hts_buf_pool_push(law_hts_pool_t *pool, law_hts_buf_t *buf)
{
        pool->list = pmt_ll_node_push_front(
                &law_hts_buf_iface, 
                pool->list, 
                buf);
}

void law_hts_stk_pool_free(law_hts_pool_t *pool)
{
        law_hts_stk_t *stk = NULL;
        while((stk = pmt_ll_node_remove_first(
                &law_hts_stk_iface, 
                (void**)&pool->list))) 
        {
                law_hts_stk_destroy(stk);
        }
}

law_hts_pool_t *law_hts_stk_pool_init(
        law_hts_pool_t *pool,
        const size_t pool_size, 
        const size_t stk_length)
{
        memset(pool, 0, sizeof(law_hts_pool_t));

        law_hts_stk_t *stk = NULL;

        for(int n = 0; n < pool_size; ++n) {
                if(!(stk = law_hts_stk_create(stk_length))) {
                        law_hts_stk_pool_free(pool);
                        return NULL;
                }
                pool->list = pmt_ll_node_push_front(
                        &law_hts_stk_iface, 
                        pool->list, 
                        stk);
        }

        return pool;
}

law_hts_stk_t *law_hts_stk_pool_pop(law_hts_pool_t *pool)
{
        return pmt_ll_node_remove_first(
                &law_hts_stk_iface, 
                &pool->list);
}

void law_hts_stk_pool_push(law_hts_pool_t *pool, law_hts_stk_t *stk)
{
        pool->list = pmt_ll_node_push_front(
                &law_hts_stk_iface, 
                pool->list, 
                stk);
}

law_hts_pool_group_t *law_hts_pool_group_create(
        law_server_cfg_t *server_cfg, 
        law_htserver_cfg_t *htserver_cfg)
{
        law_hts_pool_group_t *grp = calloc(1, sizeof(law_hts_pool_group_t));

        if(!law_hts_stk_pool_init(
                &grp->heap_pool,
                (size_t)server_cfg->worker_tasks,
                htserver_cfg->heap))
                goto FREE_GROUP;

        if(!law_hts_stk_pool_init(
                &grp->stack_pool,
                (size_t)server_cfg->worker_tasks,
                htserver_cfg->stack))
                goto FREE_HEAP_POOL;
        
        if(!law_hts_buf_pool_init(
                &grp->in_pool,
                (size_t)server_cfg->worker_tasks,
                htserver_cfg->in))
                goto FREE_STACK_POOL;

        if(!law_hts_buf_pool_init(
                &grp->out_pool,
                (size_t)server_cfg->worker_tasks,
                htserver_cfg->out))
                goto FREE_IN_POOL;
        
        return grp;

        FREE_IN_POOL:
        (void)law_hts_buf_pool_free(&grp->in_pool);

        FREE_STACK_POOL:
        (void)law_hts_stk_pool_free(&grp->stack_pool);        

        FREE_HEAP_POOL:
        (void)law_hts_stk_pool_free(&grp->heap_pool);

        FREE_GROUP:
        (void)free(grp);

        return NULL;
}

void law_hts_pool_group_destroy(law_hts_pool_group_t *grp)
{
        if(!grp) return;
        (void)law_hts_buf_pool_free(&grp->out_pool);
        (void)law_hts_buf_pool_free(&grp->in_pool);
        (void)law_hts_stk_pool_free(&grp->stack_pool);        
        (void)law_hts_stk_pool_free(&grp->heap_pool);
        (void)free(grp);
}

law_hts_pool_group_t **law_hts_pool_groups_create(
        law_server_cfg_t *srv_cfg, 
        law_htserver_cfg_t *hts_cfg) 
{
        law_hts_pool_group_t **groups = calloc(
                (size_t)srv_cfg->workers, 
                sizeof(void*));
        if(!groups) return NULL;

        int n = 0;
        for(; n < srv_cfg->workers; ++n) {
                if(!(groups[n] = law_hts_pool_group_create(
                        srv_cfg, 
                        hts_cfg)))
                        goto FAILURE;
        }

        return groups;

        FAILURE:

        for(int m = 0; m < n; ++m) {
                (void)law_hts_pool_group_destroy(groups[m]);
        }
        
        (void)free(groups);
        
        return NULL;
}

void law_hts_pool_groups_destroy(
        law_hts_pool_group_t **groups, 
        const int ngroups)
{
        for(int n = 0; n < ngroups; ++n) {
                (void)law_hts_pool_group_destroy(groups[n]);
        }
        (void)free(groups);
}

law_htserver_t *law_htserver_create(
        law_server_cfg_t *srv_cfg,
        law_htserver_cfg_t *hts_cfg)
{
        law_htserver_t *srv = calloc(1, sizeof(law_htserver_t));
        if(!srv) return NULL;

        srv->cfg = *hts_cfg;
        srv->ssl_ctx = NULL;
        srv->workers = srv_cfg->workers;

        if(!(srv->groups = law_hts_pool_groups_create(srv_cfg, hts_cfg))) {
                (void)free(srv);
                return NULL;
        }

        return srv;
}

void law_htserver_destroy(law_htserver_t *server)
{
        (void)law_hts_pool_groups_destroy(server->groups, server->workers);
        (void)free(server);
}

static sel_err_t law_hts_init_ssl_ctx(law_htserver_t *server)
{
        ERR_clear_error();
        SSL_CTX *context = SSL_CTX_new(TLS_server_method());
        if(!context) 
                return LAW_ERR_PUSH(LAW_ERR_SSL, "SSL_CTX_new");

        ERR_clear_error();
        int err = SSL_CTX_use_certificate_file(
                context, 
                server->cfg.cert, 
                SSL_FILETYPE_PEM);
        if(err < 0) {
                (void)SSL_CTX_free(context);
                return LAW_ERR_PUSH(
                        LAW_ERR_SSL, 
                        "SSL_CTX_use_certificate_file");
        }

        ERR_clear_error();
        err = SSL_CTX_use_PrivateKey_file(
                context, 
                server->cfg.pkey, 
                SSL_FILETYPE_PEM);
        if(err < 0) {
                (void)SSL_CTX_free(context);
                return LAW_ERR_PUSH(
                        LAW_ERR_SSL, 
                        "SSL_CTX_use_PrivateKey_file");
        }

        server->ssl_ctx = context;

        return LAW_ERR_OK;
}

sel_err_t law_htserver_open(law_htserver_t *server)
{
        return law_hts_init_ssl_ctx(server);
}

sel_err_t law_htserver_close(law_htserver_t *server)
{
        (void)SSL_CTX_free(server->ssl_ctx);
        return LAW_ERR_OK;
}

sel_err_t law_hts_entry_handler(law_worker_t *worker, law_hts_req_t *req)
{
        law_htserver_t *hts = req->htserver;

        law_hts_reqline_t reqline;
        (void)memset(&reqline, 0, sizeof(law_hts_reqline_t));

        law_htheaders_t headers;
        (void)memset(&headers, 0, sizeof(law_htheaders_t));

        sel_err_t err = -1;

        switch((err = law_hts_read_reqline_sync(
                worker, 
                hts->cfg.reqline_timeout, 
                req, 
                &reqline,
                pgc_buf_tell(req->conn.in))))
        {
                case LAW_ERR_OK:
                        break;
                case LAW_ERR_TIME:
                        LAW_ERR_PUSH(err, "law_hts_read_reqline_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_REQLINE_TIMEOUT,
                                hts->cfg.data);
                        return LAW_ERR_OK;
                case LAW_ERR_SYN:
                        LAW_ERR_PUSH(err, "law_hts_read_reqline_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_REQLINE_MALFORMED,
                                hts->cfg.data);
                        return LAW_ERR_OK;
                case LAW_ERR_OOB:
                        LAW_ERR_PUSH(err, "law_hts_read_reqline_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_REQLINE_TOO_LONG,
                                hts->cfg.data);
                        return LAW_ERR_OK;
                default:
                        LAW_ERR_PUSH(err, "law_hts_read_reqline_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                err,
                                hts->cfg.data);
                        return err;
        }

        switch((err = law_hts_read_headers_sync(
                worker, 
                hts->cfg.headers_timeout, 
                req, 
                &headers,
                pgc_buf_tell(req->conn.in))))
        {
                case LAW_ERR_OK:
                        break;
                case LAW_ERR_TIME:
                        LAW_ERR_PUSH(err, "law_hts_read_headers_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_HEADERS_TIMEOUT,
                                hts->cfg.data);
                        return LAW_ERR_OK;
                case LAW_ERR_SYN:
                        LAW_ERR_PUSH(err, "law_hts_read_headers_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_HEADERS_MALFORMED,
                                hts->cfg.data);
                        return LAW_ERR_OK;
                case LAW_ERR_OOB:
                        LAW_ERR_PUSH(err, "law_hts_read_headers_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_HEADERS_TOO_LONG,
                                hts->cfg.data);
                        return LAW_ERR_OK;
                default:
                        LAW_ERR_PUSH(err, "law_hts_read_headers_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                err,
                                hts->cfg.data);
                        return err;
        }

        return hts->cfg.on_accept(
                worker, 
                req, 
                &reqline, 
                &headers, 
                hts->cfg.data);
}

sel_err_t law_hts_entry_stream(law_worker_t *worker, law_hts_req_t *req)
{
        return law_hts_entry_handler(worker, req);
}

void law_hts_entry_setup(law_worker_t *worker, law_hts_req_t *req)
{
        SEL_ASSERT(req && req->htserver && req->htserver->ssl_ctx);

        law_htserver_t *hts = req->htserver;

        law_err_clear();

        sel_err_t err = -1;

        if((err = law_ectl(
                worker, 
                req->conn.socket, 
                LAW_EV_ADD, 
                0, 
                0, 
                0)) != LAW_ERR_OK) 
        {
                LAW_ERR_PUSH(err, "law_ectl");
                hts->cfg.on_error(
                        worker, 
                        req, 
                        err,
                        hts->cfg.data);
                goto CLOSE_SOCKET;
        }

        if(hts->cfg.security != LAW_HTC_SSL) {
                (void)law_hts_entry_stream(worker, req);
                goto DELETE_SOCKET;
        }

        switch((err = law_htc_ssl_accept(&req->conn, hts->ssl_ctx))) {
                case LAW_ERR_OK:
                case LAW_ERR_WANTW: 
                case LAW_ERR_WANTR: 
                        break;
                default:
                        LAW_ERR_PUSH(err, "law_htc_ssl_accept");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_SSL_ACCEPT_FAILED,
                                hts->cfg.data);
                        goto DELETE_SOCKET;
        }

        if(law_hts_entry_stream(worker, req) != LAW_ERR_OK) {
                goto SSL_FREE;
        }

        switch((err = law_htc_ssl_shutdown_sync(
                worker, 
                hts->cfg.ssl_shutdown_timeout, 
                &req->conn))) 
        {
                case LAW_ERR_OK:
                        break;
                case LAW_ERR_TIME: 
                        LAW_ERR_PUSH(err, "law_htc_ssl_shutdown_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_SSL_SHUTDOWN_TIMEOUT,
                                hts->cfg.data);
                        break;
                default:
                        LAW_ERR_PUSH(err, "law_htc_ssl_shutdown_sync");
                        hts->cfg.on_error(
                                worker, 
                                req, 
                                LAW_ERR_SSL_SHUTDOWN_FAILED,
                                hts->cfg.data);
        }

        SSL_FREE:

        (void)law_htc_ssl_free(&req->conn);

        DELETE_SOCKET:

        if((err = law_ectl(
                worker, 
                req->conn.socket, 
                LAW_EV_DEL, 
                0, 
                0, 
                0)) != LAW_ERR_OK) 
        {
                LAW_ERR_PUSH(err, "law_ectl");
                hts->cfg.on_error(
                        worker, 
                        req, 
                        err,
                        hts->cfg.data);
        }

        CLOSE_SOCKET:

        close(req->conn.socket);
}

sel_err_t law_hts_entry(
        law_worker_t *worker,
        int socket,
        law_data_t data)
{
        law_htserver_t *server = data.ptr;
        law_htserver_cfg_t *cfg = &server->cfg;

        law_hts_req_t req;
        (void)memset(&req, 0, sizeof(law_hts_req_t));

        law_hts_pool_group_t *grp = server->groups[law_get_worker_id(worker)];

        law_hts_buf_t *in = law_hts_buf_pool_pop(&grp->in_pool);
        law_hts_buf_t *out = law_hts_buf_pool_pop(&grp->out_pool);
        law_hts_stk_t *heap = law_hts_stk_pool_pop(&grp->heap_pool);
        law_hts_stk_t *stack = law_hts_stk_pool_pop(&grp->stack_pool);

        SEL_ASSERT(in && out && heap && stack);

        req.htserver = server;
        req.conn.security = cfg->security;
        req.conn.socket = socket;
        req.conn.in = pgc_buf_zero(&in->buffer);
        req.conn.out = pgc_buf_zero(&out->buffer);
        req.heap = pgc_stk_zero(&heap->stack);
        req.stack = pgc_stk_zero(&stack->stack);

        (void)law_hts_entry_setup(worker, &req);

        (void)law_hts_buf_pool_push(&grp->in_pool, in);
        (void)law_hts_buf_pool_push(&grp->out_pool, out);
        (void)law_hts_stk_pool_push(&grp->heap_pool, heap);
        (void)law_hts_stk_pool_push(&grp->stack_pool, stack);

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