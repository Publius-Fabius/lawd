
#include "lawd/server.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

sel_err_t looper(struct law_server *srv, void *state)
{
        SEL_INFO();
        return SEL_ERR_OK;
}

sel_err_t accepter(struct law_server *srv, int socket, void *state)
{
        SEL_INFO();

        char buf[4096];

        struct pollfd *pfd = law_srv_lease(srv);
        pfd->events |= POLLIN;
        pfd->fd = socket;

        law_srv_wait(srv);
        puts("reentering");

        ssize_t len = read(socket, buf, 4096);
        if(len < 0) {
                SEL_THROW(SEL_ERR_SYS);
        }
        
        buf[len] = 0;

        char out[4096];
        const int olen = sprintf(
                out, 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %li\r\n\r\n", 
                len);

        write(socket, out, (size_t)olen);
        write(socket, buf, (size_t)len);
        
        close(socket);

        return SEL_ERR_OK; 
}

int main(int argc, char ** argv)
{
        sel_init();
        law_err_init();

        static struct law_srv_cfg cfg;
        cfg.backlog = 10;
        cfg.gid = 1000;
        cfg.uid = 1000;
        cfg.stack_guard = 0x1000;
        cfg.stack_length = 0x100000;
        cfg.protocol = LAW_SRV_TCP;
        cfg.timeout = 5000;
        cfg.port = 80;
        cfg.maxconns = 10;
        cfg.tick = looper;
        cfg.accept = accepter;

        struct law_server *srv = law_srv_create(&cfg);

        sel_err_t err = law_srv_open(srv);
        if(err != SEL_ERR_OK) {
                fprintf(stderr, "error opening server:\n");
                fprintf(stderr, "error: %s\n", sel_lookup(err)[0]);
                fprintf(stderr, "errno: %s\n", strerror(errno));
                goto CLEANUP;
        } 
        
        err = law_srv_start(srv);
        if(err != SEL_ERR_OK)
        {
                fprintf(stderr, "error starting server:\n");
                fprintf(stderr, "error: %s\n", sel_lookup(err)[0]);
                fprintf(stderr, "errno: %s\n", strerror(errno));
        }

        CLEANUP:

        law_srv_close(srv);
        law_srv_destroy(srv);
}