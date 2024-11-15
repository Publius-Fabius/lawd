
#include "lawd/webd.h"
#include "lawd/log.h"
#include "lawd/websock.h"
#include <stdlib.h>
#include <stdint.h>
#include <openssl/types.h>
#include <openssl/evp.h>

sel_err_t ws_handler(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *request,
        struct law_hts_head *head,
        struct law_data data)
{
        SEL_INFO();

        struct law_hthdrs *hdrs = head->headers;
        const int sock = request->conn.socket;
        FILE *errs = law_srv_errors(law_srv_server(worker));
        const char *key = law_hth_get(hdrs, "Sec-WebSocket-Key");
        if(!key) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_hth_get", 
                        "Sec-WebSocket-Key Is Missing");
                return LAW_ERR_OK;
        }

        SEL_TRY(law_ws_accept(worker, request, head));

        law_htc_flush(&request->conn);
        
        law_srv_sleep(worker, 5000);

        struct law_ws_head wsh;
        struct law_ws_xor xor;

        memset(&wsh, 0, sizeof(struct law_ws_head));
        law_ws_xor_gen(&xor);

        wsh.fin = 1;
        wsh.masking_key = xor.masking_key;
        wsh.opcode = 1;
        wsh.payload_length = 5;
        
        law_ws_write_head(&request->conn, &wsh);

        char msg[6] = "hello";
        pgc_buf_put(request->conn.out, law_ws_xor_apply(&xor, msg, 5), 5);

        law_htc_flush(&request->conn);
        puts("MSG SENT");

        law_srv_sleep(worker, 1000);
        request->conn.in->offset = request->conn.in->end;
        memset(&wsh, 0, sizeof(struct law_ws_head));
        SEL_TRY(law_ws_read_head_start(&request->conn, &wsh));

        SEL_ASSERT(wsh.fin);
        SEL_ASSERT(wsh.payload_length == 5);
        SEL_TRY(law_ws_read_head_small(&request->conn, &wsh));
        law_ws_xor_init(&xor, &wsh.masking_key);

        pgc_buf_get(request->conn.in, msg, 5);
        law_ws_xor_apply(&xor, msg, 5);
        SEL_ASSERT(!strcmp(msg, "hello"));

        puts("SUCCESS");
        law_srv_sleep(worker, 1000);

        return LAW_ERR_OK;
}

sel_err_t ws_onerror(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *req,
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
        struct law_wd_cfg wd_cfg = law_wd_sanity();
        wd_cfg.handler = ws_handler;
        wd_cfg.onerror = ws_onerror;
        struct law_webd *webd = law_wd_create(&wd_cfg);       

        struct law_hts_cfg ht_cfg = law_hts_sanity();
        ht_cfg.callback = law_wd_accept;
        ht_cfg.data.u.ptr = webd;
        struct law_hts_ctx *sctx = law_hts_create(&ht_cfg);

        struct law_srv_cfg srv_cfg = law_srv_sanity();
        srv_cfg.data.u.ptr = sctx;
        srv_cfg.accept = law_hts_accept;
        srv_cfg.init = ws_init;
        srv_cfg.tick = ws_tick;
        
        struct law_server *srv = law_srv_create(&srv_cfg);
        
        if(law_srv_open(srv) == LAW_ERR_OK)
                law_srv_start(srv);

        law_srv_destroy(srv);
        law_hts_destroy(sctx);
        law_wd_destroy(webd);

        return EXIT_SUCCESS;
}