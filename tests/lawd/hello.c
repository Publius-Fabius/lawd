
#include <stdlib.h>
#include <stdio.h>
#include "lawd/http.h"

sel_err_t looper(struct law_srv *srv, void *st) 
{
        puts("looper");
        return LAW_ERR_OK;
}

sel_err_t handler(struct law_srv *srv, struct law_ht_sreq *req)
{
        enum law_ht_meth method;
        enum law_ht_vers version;
        struct law_uri target;
        struct law_ht_hdrs *headers;

        struct pollfd *pfd;

        for(;;) {
                /* Test for timeouts here. */
                sel_err_t err = law_ht_sreq_read_head(
                        req, 
                        &method, 
                        &version, 
                        &target, 
                        &headers);
                if(err == LAW_ERR_AGAIN) {
                        pfd = law_srv_lease(srv);
                        pfd->events = POLLIN | POLLHUP;
                        pfd->fd = law_ht_sreq_socket(req);
                        puts("yielding");
                        law_srv_yield(srv);
                } else if(err != LAW_ERR_OK) {
                        /* Send 400 here. */
                        law_ht_sreq_done(req);
                        fprintf(stderr, 
                                "error reading request head: %s\r\n", 
                                sel_lookup(err)[0]);
                        /* Error other than OK will stop the server. */
                        return LAW_ERR_OK;
                } else {
                        break;
                }
        }

        puts(law_ht_meth_str(method));
        puts(law_ht_vers_str(version));
        puts(target.path);

        const char *name;
        const char *value;

        struct law_ht_hdrs_i *i = law_ht_hdrs_elems(headers);
        while(law_ht_hdrs_i_next(i, &name, &value)) {
                printf("%s: %s\r\n", name, value);
        }

        SEL_TRY(law_ht_sreq_write_head(
                req,
                200,
                2,
                (const char *[][2]){ 
                        { "Content-Length", "3" },
                        { "Content-Type", "text/plain" }
                }));
        
        struct pgc_buf *out = law_ht_sreq_out(req);

        pgc_buf_put(out, "abc", 3);

        for(;;) {
                sel_err_t err = law_ht_sreq_write_data(req);
                if(err == LAW_ERR_AGAIN) {
                        pfd = law_srv_lease(srv);
                        pfd->events = POLLOUT | POLLHUP;
                        pfd->fd = law_ht_sreq_socket(req);
                        law_srv_yield(srv);
                } else if(err != LAW_ERR_OK) {
                        /* Send 500 here. */
                        law_ht_sreq_done(req);
                        fprintf(stderr, 
                                "error writing response: %s\r\n", 
                                sel_lookup(err)[0]);
                        return LAW_ERR_OK;
                } else {
                        break;
                }
        }
        
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
        struct law_ht_sctx *ctx = law_ht_sctx_create(&ctx_cfg);

        static struct law_srv_cfg srv_cfg;
        srv_cfg.backlog = 10;
        srv_cfg.gid = 1000;
        srv_cfg.uid = 1000;
        srv_cfg.stack_guard = 0x1000;
        srv_cfg.stack_length = 0x100000;
        srv_cfg.protocol = LAW_SRV_TCP;
        srv_cfg.timeout = 5000;
        srv_cfg.port = 80;
        srv_cfg.maxconns = 10;
        srv_cfg.tick = looper;
        srv_cfg.accept = law_ht_accept;
        srv_cfg.state = ctx;
        struct law_srv *srv = law_srv_create(&srv_cfg);
        
        sel_err_t err;
        err = law_srv_open(srv);
        if(err != LAW_ERR_OK) {
                fprintf(stderr, 
                        "error opening server:%s\r\n", 
                        sel_lookup(err)[0]);
                goto CLEANUP;
        }

        if(law_srv_start(srv) != LAW_ERR_OK) {
                fprintf(stderr, 
                        "error starting server:%s\r\n", 
                        sel_lookup(err)[0]);
                goto CLEANUP;
        }

        law_srv_close(srv);

        CLEANUP:

        law_ht_sctx_destroy(ctx);
        law_srv_destroy(srv);
 
        return EXIT_SUCCESS;
}