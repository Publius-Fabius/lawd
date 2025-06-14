
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

        // char in[1024];
        char out[1024];

        const ssize_t olen = sprintf(
                out, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 4\r\n\r\n");

        struct law_slot slot = { 
                .fd = sock, 
                .task = law_srv_active(w) };

        SEL_TEST(law_ectl(
                w, &slot, LAW_EV_ADD, 0, LAW_EV_R) == LAW_ERR_OK);

        struct law_event evs[5];

        SEL_TEST(law_ewait(w, law_time_millis() + 5000, evs, 5) > 0);
        SEL_TEST(evs[0].events & LAW_EV_R);

        SEL_TEST(law_ectl(w, &slot, LAW_EV_DEL, 0, 0) == LAW_ERR_OK);
        SEL_TEST(law_ewait(w, law_time_millis() + 1000, evs, 5) > 0);
        SEL_TEST(evs[0].events & LAW_EV_TIM);

        write(sock, out, (size_t)olen);
        write(sock, "pong", 4);
        close(sock);

        return LAW_ERR_OK; 
}

int main(int argc, char ** argv)
{
        sel_init();
        law_err_init();

        /* Can't control when client hangs up. */
        signal(SIGPIPE, SIG_IGN);

        int data = 0;

        struct law_server_config cfg = law_server_sanity();
        cfg.init = initer;
        cfg.tick = ticker;
        cfg.accept = accepter;
        cfg.data.ptr = &data;
        struct law_server *srv = law_server_create(&cfg);

        sel_err_t err = law_open(srv);
        if(err != SEL_ERR_OK) goto CLEANUP;
        
        err = law_start(srv);

        CLEANUP:

        law_close(srv);
        law_server_destroy(srv);

        return EXIT_SUCCESS;
}