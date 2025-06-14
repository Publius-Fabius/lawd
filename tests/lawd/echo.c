
#include "lawd/webd.h"

sel_err_t echo_init(struct law_worker *worker, struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;
}

sel_err_t echo_tick(struct law_worker *worker, struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;
}

sel_err_t echo_onerror(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *req,
        const int status,
        struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;
}

sel_err_t echo_handler(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *req,
        struct law_hts_head *head,
        struct law_data data)
{ 
        struct pgc_stk *heap = req->heap;

        struct pgc_buf buf;
        char *raw_buf = pgc_stk_push(heap, 1024);
        if(!raw_buf) 
                return 500;
        pgc_buf_init(&buf, raw_buf, 1024, 0);

        SEL_TRY(pgc_buf_printf(&buf, "%s ", head->method));
        SEL_TRY(law_uri_bprint(&buf, &head->target));
        SEL_TRY(pgc_buf_printf(&buf, " %s\r\n", head->version));

        const char *name, *value;

        struct law_hthdrs_iter *i = law_hth_elems(
                head->headers,
                (void*(*)(void*, const size_t))pgc_stk_push,
                heap);
        while(law_hth_next(i, &name, &value)) {
                SEL_TRY(pgc_buf_printf(&buf, "%s:%s\r\n", name, value));
        }

        const size_t length = pgc_buf_end(&buf);
        char *str = pgc_stk_push(heap, 16);
        if(!str) 
                return 500;
        SEL_IO(snprintf(str, 16, "%zu", length));
        
        SEL_TRY(law_hts_set_status(
                req, 
                "HTTP/1.1", 
                200, 
                law_hts_status_str(200)));

        SEL_TRY(law_hts_add_header(req, "Content-Length", str));
        SEL_TRY(law_hts_add_header(req, "Content-Type", "text/plain"));
        SEL_TRY(law_hts_begin_body(req));

        SEL_TRY(pgc_buf_put(req->conn.out, raw_buf, length));
        SEL_TRY(law_htc_flush(&req->conn));

        return LAW_ERR_OK;
}

int main(int argc, char **args) 
{
        law_err_init();

        struct law_wd_cfg wd_cfg = law_wd_sanity();
        wd_cfg.handler = echo_handler;
        wd_cfg.onerror = echo_onerror;
        struct law_webd *webd = law_wd_create(&wd_cfg);       

        struct law_hts_cfg ht_cfg = law_hts_sanity();
        ht_cfg.callback = law_wd_accept;
        ht_cfg.data.u.ptr = webd;
        struct law_hts_ctx *sctx = law_hts_create(&ht_cfg);

        struct law_server_config srv_cfg = law_server_sanity();
        srv_cfg.data.u.ptr = sctx;
        srv_cfg.accept = law_hts_accept;
        srv_cfg.init = echo_init;
        srv_cfg.tick = echo_tick;
        
        struct law_server *srv = law_server_create(&srv_cfg);
        
        SEL_TRY(law_open(srv));
        law_start(srv);

        law_server_destroy(srv);
        law_hts_destroy(sctx);
        law_wd_destroy(webd);

        return EXIT_SUCCESS;
}