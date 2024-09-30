
#include "lawd/server.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

sel_err_t initer(struct law_worker *w, struct law_data *data)
{
        SEL_INFO();
        return SEL_ERR_OK;
}

sel_err_t ticker(struct law_worker *w, struct law_data *data)
{
        SEL_INFO();
        return SEL_ERR_OK;
}

sel_err_t accepter(struct law_worker *w, int sock, struct law_data *data)
{
        SEL_INFO();

        char out[4096];
        const int olen = sprintf(
                out, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 4\r\n\r\n");

        write(sock, out, (size_t)olen);
        write(sock, "pong", 4);

        close(sock);

        return SEL_ERR_OK; 
}

int main(int argc, char ** argv)
{
        sel_init();
        law_err_init();

        /* Can't control when client hangs up. */
        signal(SIGPIPE, SIG_IGN);

        struct law_srv_cfg cfg = *law_srv_cfg_sanity();
        cfg.init = initer;
        cfg.tick = ticker;
        cfg.accept = accepter;

        struct law_server *srv = law_srv_create(&cfg);

        sel_err_t err = law_srv_open(srv);
        if(err != SEL_ERR_OK) {
                goto CLEANUP;
        } 
        
        err = law_srv_start(srv);

        CLEANUP:

        law_srv_close(srv);
        law_srv_destroy(srv);

        return EXIT_SUCCESS;
}