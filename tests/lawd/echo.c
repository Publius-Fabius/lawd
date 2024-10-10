
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
        struct law_ht_sreq *req,
        const int status,
        struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;      
}

sel_err_t echo_handler(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_ht_req_head *head,
        struct law_data data)
{ 
        uint8_t raw_buf[0x2000];
        struct pgc_buf buf;
        pgc_buf_init(&buf, raw_buf, 0x2000, 0);

        SEL_TRY(pgc_buf_printf(&buf, "%s ", head->method));
        SEL_TRY(law_uri_bprint(&buf, &head->target));
        SEL_TRY(pgc_buf_printf(&buf, " %s\r\n", head->version));

        const char *name, *value;

        struct law_ht_hdrs_i *i = law_ht_hdrs_elems(head->headers);
        while(law_ht_hdrs_i_next(i, &name, &value)) {
                SEL_TRY(pgc_buf_printf(&buf, "%s:%s\r\n", name, value));
        }

        char str[16];
        const size_t length = pgc_buf_end(&buf);

        SEL_TRY(law_ht_sreq_set_status(
                req, 
                "HTTP/1.1", 
                200, 
                law_ht_status_str(200)));

        SEL_TRY(law_ht_sreq_add_header(req, "Content-Length", str));
        SEL_TRY(law_ht_sreq_add_header(req, "Content-Type", "text/plain"));
        SEL_TRY(law_ht_sreq_begin_body(req));

        SEL_TRY(pgc_buf_put(law_ht_sreq_out(req), raw_buf, length));

        SEL_TRY(law_wd_flush(req));

        return LAW_ERR_OK;
}

int main(int argc, char **args) 
{
        law_err_init();

        struct law_wd_cfg wd_cfg = *law_wd_cfg_sanity();
        wd_cfg.handler = echo_handler;
        wd_cfg.onerror = echo_onerror;
        struct law_webd *webd = law_wd_create(&wd_cfg);       

        struct law_ht_sctx_cfg ht_cfg = *law_ht_sctx_cfg_sanity();
        ht_cfg.callback = law_wd_accept;
        ht_cfg.data.u.ptr = webd;
        struct law_ht_sctx *sctx = law_ht_sctx_create(&ht_cfg);

        struct law_srv_cfg srv_cfg = *law_srv_cfg_sanity();
        srv_cfg.data.u.ptr = sctx;
        srv_cfg.accept = law_ht_accept;
        srv_cfg.init = echo_init;
        srv_cfg.tick = echo_tick;
        
        struct law_server *srv = law_srv_create(&srv_cfg);
        
        SEL_TRY(law_srv_open(srv));
        law_srv_start(srv);

        law_srv_destroy(srv);
        law_ht_sctx_destroy(sctx);
        law_wd_destroy(webd);

        return EXIT_SUCCESS;
}