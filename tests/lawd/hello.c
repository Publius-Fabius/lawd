
#include <stdlib.h>
#include <stdio.h>
#include "lawd/http.h"
#include "openssl/err.h"

sel_err_t looper(struct law_srv *srv, void *st) 
{
      //  puts("looper");
        return LAW_ERR_OK;
}

sel_err_t handler(struct law_srv *srv, struct law_ht_sreq *req)
{
        sel_err_t err = law_ht_sreq_ssl_accept(req);
        if(err == LAW_ERR_SSL) {
                fprintf(stderr, 
                        "%s\r\n", 
                        ERR_error_string(ERR_get_error(), NULL));
                return LAW_ERR_OK;
        }

        for(int x = 0; x < 2; ++x) {
                law_srv_yield(srv);
        }

        law_ht_sreq_set_status(
                req, 
                "HTTP/1.1", 
                200, 
                law_ht_status_str(200));

        law_ht_sreq_add_header(req, "Content-Length", "3");
        law_ht_sreq_add_header(req, "Content-Type", "text/plain");
        law_ht_sreq_body(req);

        struct pgc_buf *out = law_ht_sreq_out(req);
        pgc_buf_put(out, "abc", 3);
        
        for(int x = 0; x < 2; ++x) {
                law_ht_sreq_write_data(req);
                law_srv_yield(srv);
        }

        law_ht_sreq_ssl_shutdown(req);
        law_ht_sreq_done(req);

        return LAW_ERR_OK;
}

int main(int argc, char ** args) 
{
        law_err_init();

        static struct law_ht_sctx_cfg ctx_cfg;
        ctx_cfg.in_length = 0x3000;
        ctx_cfg.in_guard = 0x1000;
        ctx_cfg.out_length = 0x3000;
        ctx_cfg.out_guard = 0x1000;
        ctx_cfg.heap_length = 0xF000;
        ctx_cfg.heap_guard = 0x1000;
        ctx_cfg.handler = handler;
        ctx_cfg.security = LAW_HT_SSL;
        ctx_cfg.certificate = "tmp/cert.pem";
        ctx_cfg.private_key = "tmp/key.pem";

        struct law_ht_sctx *ctx = law_ht_sctx_create(&ctx_cfg);

        sel_err_t err = law_ht_sctx_init(ctx);
        if(err == LAW_ERR_SSL) {
                fprintf(stderr, 
                        "%s\r\n",
                        ERR_error_string(ERR_get_error(), NULL));
                goto CLEANUP;
        }

        static struct law_srv_cfg srv_cfg;
        srv_cfg.backlog = 10;
        srv_cfg.gid = 1000;
        srv_cfg.uid = 1000;
        srv_cfg.stack_guard = 0x1000;
        srv_cfg.stack_length = 0x100000;
        srv_cfg.protocol = LAW_SRV_TCP;
        srv_cfg.timeout = 1000;
        srv_cfg.port = 443;
        srv_cfg.maxconns = 10;
        srv_cfg.tick = looper;
        srv_cfg.accept = law_ht_accept;
        srv_cfg.state = ctx;
        struct law_srv *srv = law_srv_create(&srv_cfg);
        
        err = law_srv_open(srv);
        if(err != LAW_ERR_OK) {
                fprintf(stderr, 
                        "error opening server:%s\r\n", 
                        sel_strerror(err));
                goto CLEANUP;
        }

        if(law_srv_start(srv) != LAW_ERR_OK) {
                fprintf(stderr, 
                        "error starting server:%s\r\n", 
                        sel_strerror(err));
                goto CLEANUP;
        }

        law_srv_close(srv);

        CLEANUP:

        law_ht_sctx_destroy(ctx);
        law_srv_destroy(srv);
 
        return EXIT_SUCCESS;
}