
#include "lawd/server.h"
#include "lawd/time.h"
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

        char out[1024];

        int olen = sprintf(
                out, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 4\r\n\r\n");

        if(law_ectl(w, sock, LAW_EV_ADD, 0, LAW_EV_R, -5) != LAW_ERR_OK) {
                perror("");
                SEL_HALT();
        }

        struct law_event evs[5];       

        SEL_TEST(law_ewait(w, 3000, evs, 5) > 0);
        SEL_TEST(evs[0].events & LAW_EV_R);
        SEL_TEST(evs[0].data.i8 == -5);

        SEL_TEST(law_ectl(w, sock, LAW_EV_DEL, 0, 0, 0) == LAW_ERR_OK);
        SEL_TEST(law_ewait(w, 1000, evs, 3) > 0);
        SEL_TEST(evs[0].events & LAW_EV_TIM);

        write(sock, out, (size_t)olen);
        write(sock, "pong", 4);
        close(sock);

        return LAW_ERR_OK; 
}

int main(int argc, char ** argv)
{
        law_error_init();

        signal(SIGPIPE, SIG_IGN);

        int data = 0;

        law_server_config_t cfg = law_server_sanity();
        cfg.init = initer;
        cfg.tick = ticker;
        cfg.accept = accepter;
        cfg.data.ptr = &data;
        law_server_t *srv = law_server_create(&cfg);

        sel_err_t err;

        if((err = law_create_socket(srv, NULL)) != LAW_ERR_OK) {
                SEL_REPORT(err);
                goto FAILURE;
        }
                
        if((err = law_bind_socket(srv)) != LAW_ERR_OK) {
                SEL_REPORT(err);
                goto FAILURE;
        }

        if((err = law_listen(srv)) != LAW_ERR_OK) {
                SEL_REPORT(err);
                goto FAILURE;
        }
        
        if((err = law_open(srv)) != LAW_ERR_OK) {
                SEL_REPORT(err);
                goto FAILURE;
        }

        if((err = law_start(srv)) != LAW_ERR_OK) {
                SEL_REPORT(err);
                goto FAILURE;
        }

        law_close(srv);
        law_server_destroy(srv);

        return EXIT_SUCCESS;

        FAILURE:
        
        perror("");

        law_close(srv);
        law_server_destroy(srv);

        return EXIT_FAILURE;
}