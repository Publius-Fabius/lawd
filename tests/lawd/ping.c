
#include "lawd/server.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

sel_err_t initer(struct law_worker *w, struct law_data data)
{
        SEL_INFO();
        return SEL_ERR_OK;
}

sel_err_t ticker(struct law_worker *w, struct law_data data)
{
        SEL_INFO();
        return SEL_ERR_OK;
}

sel_err_t accepter(struct law_worker *w, int sock, struct law_data data)
{
        SEL_INFO();

        char out[4096];
        const ssize_t olen = sprintf(
                out, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 4\r\n\r\n");

        struct law_event ev;
        ev.events = LAW_SRV_PIN;
        ev.fd = sock;
        if(law_srv_watch(w, sock) != LAW_ERR_OK) {
                perror("law_srv_watch");
                SEL_ABORT();
        }
        law_srv_poll(w, 1000, 1, &ev);
        law_srv_unwatch(w, sock);

        SEL_TEST(ev.revents & LAW_SRV_PIN);

        write(sock, out, (size_t)olen);
        write(sock, "pong", 4);
        close(sock);

        if((*(int*)(data.u.ptr))++ >= 5) {
                law_srv_stop(law_srv_server(w));
        }
        
        return LAW_ERR_OK; 
}

int main(int argc, char ** argv)
{
        sel_init();
        law_err_init();

        /* Can't control when client hangs up. */
        signal(SIGPIPE, SIG_IGN);

        int data = 0;

        struct law_srv_cfg cfg = *law_srv_cfg_sanity();
        cfg.init = initer;
        cfg.tick = ticker;
        cfg.accept = accepter;
        cfg.data.u.ptr = &data;
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