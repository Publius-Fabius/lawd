
#include "lawd/webd.h"
#include "lawd/log.h"
#include "lawd/websock.h"

#include <stdlib.h>
#include <stdint.h>
#include <openssl/types.h>
#include <openssl/evp.h>

sel_err_t ws_ensure_input(
        struct law_worker *worker, 
        struct law_ht_sreq *req, 
        const size_t nbytes)
{

}

sel_err_t ws_handler(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *request,
        struct law_ht_req_head *head,
        struct law_data data)
{
        SEL_INFO();

        struct law_ht_hdrs *hdrs = head->headers;

        const char *key = law_ht_hdrs_get(hdrs, "Sec-WebSocket-Key");
        if(!key) {
                law_wd_log_error(
                        worker, 
                        request, 
                        "law_ht_hdrs_get", 
                        "Sec-WebSocket-Key Is Missing");
                return LAW_ERR_OK;
        }

        struct law_ws_accept_buf acc_buf;
        const char *accept = law_ws_gen_accept(&acc_buf, key);



        return LAW_ERR_OK;
}

sel_err_t ws_onerror(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *req,
        const int status,
        struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;
}

sel_err_t ws_init(struct law_worker *worker, struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;
}

sel_err_t ws_tick(struct law_worker *worker, struct law_data data)
{
        SEL_INFO();
        return LAW_ERR_OK;
}

int main(int argc, char **args)
{
        struct law_wd_cfg wd_cfg = *law_wd_cfg_sanity();
        wd_cfg.handler = ws_handler;
        wd_cfg.onerror = ws_onerror;
        struct law_webd *webd = law_wd_create(&wd_cfg);       

        struct law_ht_sctx_cfg ht_cfg = *law_ht_sctx_cfg_sanity();
        ht_cfg.callback = law_wd_accept;
        ht_cfg.data.u.ptr = webd;
        struct law_ht_sctx *sctx = law_ht_sctx_create(&ht_cfg);

        struct law_srv_cfg srv_cfg = *law_srv_cfg_sanity();
        srv_cfg.data.u.ptr = sctx;
        srv_cfg.accept = law_ht_accept;
        srv_cfg.init = ws_init;
        srv_cfg.tick = ws_tick;
        
        struct law_server *srv = law_srv_create(&srv_cfg);
        
        law_srv_open(srv);
        law_srv_start(srv);

        law_srv_destroy(srv);
        law_ht_sctx_destroy(sctx);
        law_wd_destroy(webd);

        return EXIT_SUCCESS;
}