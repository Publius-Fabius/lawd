
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

enum law_srv_mode {
        LAW_SRV_READY           = 0,            /** Ready */
        LAW_SRV_SUSPENDED       = 1,            /** Suspended */
        LAW_SRV_RUNNING         = 2,            /** Running */
        LAW_SRV_CLOSED          = 3             /** Closed */
};

struct law_srv_conn {                           /** Network Connection */
        enum law_srv_mode mode;                 /** Connection Mode */
        int socket;                             /** Socket Descriptor */
        struct law_cor *callee;                 /** Coroutine Environment */
        struct law_smem *stack;                 /** Coroutine Stack */
};

struct law_srv_pfds {
        struct pollfd *array;                   /** Pollfd Array */
        size_t size;                            /** Current Size */
        size_t capacity;                        /** Current Capacity */
};

struct law_srv_conns {
        struct law_srv_conn **array;            /** Conn Ptr Array */
        size_t size;                            /** Current Size */
        size_t capacity;                        /** Current Capacity */
};

struct law_srv {
        enum law_srv_mode mode;                 /** Server Mode */
        int socket;                             /** Server Socket */
        struct law_cor *caller;                 /** Calling Environment */
        struct law_srv_conn *target;            /** Target Connection */
        struct law_srv_conns *conns[2];         /** Connection Dbl Buf */
        struct law_srv_pfds *pfds[2];           /** Pollfd Dbl Buf */
        struct law_srv_cfg *cfg;                /** Configuration */
};

struct law_srv_pfds *law_srv_pfds_create()
{
        struct law_srv_pfds *ps = malloc(sizeof(struct law_srv_pfds));
        ps->size = 0;
        ps->capacity = 1;
        ps->array = calloc(ps->capacity, sizeof(struct pollfd));
        return ps;
}

void law_srv_pfds_destroy(struct law_srv_pfds *ps)
{
        free(ps->array);
        free(ps);
}

struct law_srv_pfds *law_srv_pfds_grow(struct law_srv_pfds *ps)
{
        const size_t old_capacity = ps->capacity;
        const size_t new_capacity = old_capacity * 2;
        ps->array = realloc(ps->array, new_capacity * sizeof(struct pollfd));
        memset( ps->array + old_capacity, 
                0, 
                (new_capacity - old_capacity) * sizeof(struct pollfd));
        ps->capacity = new_capacity;
        return ps;
}

struct pollfd *law_srv_pfds_take(struct law_srv_pfds *ps)
{
        if(ps->size == ps->capacity) {
                law_srv_pfds_grow(ps);
        }
        return ps->array + ps->size++;
}

struct law_srv_pfds *law_srv_pfds_reset(struct law_srv_pfds *ps)
{
        memset(ps->array, 0, ps->capacity * sizeof(struct pollfd));
        ps->size = 0;
        return ps;
}

size_t law_srv_pfds_size(struct law_srv_pfds *ps)
{
        return ps->size;
}

size_t law_srv_pfds_capacity(struct law_srv_pfds *ps)
{
        return ps->capacity;
}

struct pollfd *law_srv_pfds_array(struct law_srv_pfds *ps)
{
        return ps->array;
}

struct law_srv_conns *law_srv_conns_create()
{
        struct law_srv_conns *cs = malloc(sizeof(struct law_srv_conns));
        cs->size = 0;
        cs->capacity = 1;
        cs->array = calloc(cs->capacity, sizeof(void*));
        return cs;
}

void law_srv_conns_destroy(struct law_srv_conns *cs)
{
        free(cs->array);
        free(cs);
}

struct law_srv_conns *law_srv_conns_grow(struct law_srv_conns *cs)
{
        const size_t old_capacity = cs->capacity;
        const size_t new_capacity = old_capacity * 2;
        cs->array = realloc(cs->array, new_capacity * sizeof(void*));
        memset( cs->array + old_capacity, 
                0, 
                (new_capacity - old_capacity) * sizeof(void*));
        cs->capacity = new_capacity;
        return cs;
}

struct law_srv_conn **law_srv_conns_take(struct law_srv_conns *cs)
{
        if(cs->size == cs->capacity) {
                law_srv_conns_grow(cs);
        }
        struct law_srv_conn **ptr = cs->array + cs->size++;
        *ptr = NULL;
        return ptr;
}

struct law_srv_conns *law_srv_conns_reset(struct law_srv_conns *cs)
{
        memset(cs->array, 0, cs->capacity * sizeof(void*));
        cs->size = 0;
        return cs;
}

size_t law_srv_conns_size(struct law_srv_conns *cs)
{
        return cs->size;
}

size_t law_srv_conns_capacity(struct law_srv_conns *cs)
{
        return cs->capacity;
}

struct law_srv_conn ** law_srv_conns_array(struct law_srv_conns *cs)
{
        return cs->array;
}

struct law_srv_conn *law_srv_conn_create(
        const int socket,
        const size_t stacklen,
        const size_t guardlen)
{
        struct law_srv_conn *conn = malloc(sizeof(struct law_srv_conn));
        conn->callee = law_cor_create();
        conn->stack = law_smem_create(stacklen, guardlen);
        conn->mode = LAW_SRV_READY;
        conn->socket = socket;
        return conn;
}

void law_srv_conn_destroy(struct law_srv_conn *conn) 
{
        law_smem_destroy(conn->stack);
        law_cor_destroy(conn->callee);
        free(conn);
}

struct law_srv *law_srv_create(struct law_srv_cfg *cfg)
{
        struct law_srv *srv = malloc(sizeof(struct law_srv));
        srv->mode = LAW_SRV_READY;
        srv->socket = 0;
        srv->caller = law_cor_create();
        srv->target = NULL;
        srv->conns[0] = law_srv_conns_create();
        srv->conns[1] = law_srv_conns_create();
        srv->pfds[0] = law_srv_pfds_create();
        srv->pfds[1] = law_srv_pfds_create();
        srv->cfg = cfg;
        return srv;
}

void law_srv_destroy(struct law_srv *srv)
{
        law_srv_pfds_destroy(srv->pfds[1]);
        law_srv_pfds_destroy(srv->pfds[0]);
        law_srv_conns_destroy(srv->conns[1]);
        law_srv_conns_destroy(srv->conns[0]);
        law_cor_destroy(srv->caller);
        free(srv);
}

int law_srv_socket(struct law_srv *srv)
{
        return srv->socket;
}

void law_srv_pfds_swap(struct law_srv *srv)
{
        struct law_srv_pfds *tmp = srv->pfds[0];
        srv->pfds[0] = srv->pfds[1];
        srv->pfds[1] = tmp;
}

void law_srv_conns_swap(struct law_srv *srv)
{
        struct law_srv_conns *tmp = srv->conns[0];
        srv->conns[0] = srv->conns[1];
        srv->conns[1] = tmp;
}

struct pollfd *law_srv_lease(struct law_srv *s)
{
        return law_srv_pfds_take(s->pfds[0]);
}

int law_srv_poll(
        struct law_srv *server, 
        const int socket,
        const int events)
{
        /* Set up polling descriptor. */
        struct pollfd *pfd = law_srv_lease(server);
        pfd->fd = socket;
        pfd->events = events;
        pfd->revents = 0;

        /* Yield back to the scheduler. */
        law_srv_yield(server);

        /* Return any triggered events. */
        return pfd->revents;
}

static sel_err_t law_srv_listen(struct law_srv *s)
{
        struct sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));
        int domain, type;
        unsigned int addrlen;
        struct sockaddr_in *in;
        struct sockaddr_in6 *in6;

        switch(s->cfg->protocol) {

                /* TCP over IPv4 */
                case LAW_SRV_TCP: 
                        domain = AF_INET;
                        type = SOCK_STREAM;
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons(s->cfg->port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                break;

                /* TCP over IPv6 */
                case LAW_SRV_TCP6:
                        domain = AF_INET6;
                        type = SOCK_STREAM;
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons(s->cfg->port);
                        in6->sin6_addr = in6addr_any;
                break;

                /* UDP over IPv4 */
                case LAW_SRV_UDP:
                        domain = AF_INET;
                        type = SOCK_DGRAM;
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons(s->cfg->port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                break;

                /* UDP over IPv6 */
                case LAW_SRV_UDP6:
                        domain = AF_INET6;
                        type = SOCK_DGRAM;
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons(s->cfg->port);
                        in6->sin6_addr = in6addr_any;
                break;

                default: SEL_ABORT();
        }

        /* Initialize the socket's file descriptor. */
        const int fd = socket(domain, type, 0);
        if(fd < 0) { 
                SEL_THROW(SEL_ERR_SYS);
        }
        
        /* Get the socket's file status flags and set to non-blocking. */
        const int flags = fcntl(fd, F_GETFL);
        if(flags < 0) {
                SEL_THROW(SEL_ERR_SYS);
        } else if(fcntl(fd, F_SETFL, O_NONBLOCK | flags) < 0) {
                SEL_THROW(SEL_ERR_SYS);
        }

        /* Assign the address to the socket. */
        if(bind(fd, (struct sockaddr*)&addr, addrlen) < 0) { 
                SEL_THROW(SEL_ERR_SYS); 
        }

        /* Start listening for connections. */
        if(listen(fd, s->cfg->backlog)) { 
                SEL_THROW(SEL_ERR_SYS);
        }

        s->socket = fd;
        return SEL_ERR_OK;
}

sel_err_t law_srv_open(struct law_srv *srv)
{
        SEL_TRY_QUIETLY(law_srv_listen(srv));
        SEL_IO(setgid(srv->cfg->gid));
        SEL_IO(setuid(srv->cfg->uid));
        srv->mode = LAW_SRV_SUSPENDED;
        return SEL_ERR_OK;
}

sel_err_t law_srv_close(struct law_srv *srv)
{
        SEL_IO(close(srv->socket));
        srv->mode = LAW_SRV_CLOSED;
        return SEL_ERR_OK;
}

static sel_err_t law_srv_trampoline(
        struct law_cor *caller, 
        struct law_cor *callee,
        void *state)
{
        struct law_srv *s = state;
        return s->cfg->accept(s, s->target->socket, s->cfg->state);
}

static sel_err_t law_srv_dispatch(struct law_srv *srv)
{
        law_srv_conns_swap(srv);
        law_srv_conns_reset(srv->conns[0]);

        law_srv_pfds_swap(srv);
        law_srv_pfds_reset(srv->pfds[0]);

        struct law_srv_conns *conns1 = srv->conns[1];

        for(size_t n = 0; n < conns1->size; ++n) {
                struct law_srv_conn *conn = conns1->array[n];
                srv->target = conn;
                sel_err_t err;
                switch(conn->mode) {
                        case LAW_SRV_READY:
                                /* Connection is ready to call accept. */
                                conn->mode = LAW_SRV_RUNNING;
                                err = law_cor_call(
                                        srv->caller,
                                        conn->callee,
                                        conn->stack,
                                        law_srv_trampoline,
                                        srv); 
                                break;
                        case LAW_SRV_SUSPENDED:
                                /* Accept was previously suspended. */
                                conn->mode = LAW_SRV_RUNNING;
                                err = law_cor_resume(
                                        srv->caller,
                                        conn->callee,
                                        0);
                                break;
                        default: SEL_ABORT();
                }
                switch(err) {
                        case SEL_ERR_SYS:
                                /** Accept finished. */
                                // conn->mode = LAW_SRV_CLOSED;
                                law_srv_conn_destroy(conn);
                                break;
                        case LAW_ERR_AGAIN:
                                /** Accept is suspended until later. */
                                conn->mode = LAW_SRV_SUSPENDED;
                                *law_srv_conns_take(srv->conns[0]) = conn;
                                break;
                        default: 
                                /** An error occurred. */
                                // conn->mode = LAW_SRV_CLOSED;
                                law_srv_conn_destroy(conn);
                                return err;
                }
        }

        return SEL_ERR_OK;
}

static sel_err_t law_srv_accept(struct law_srv *srv)
{
        while(srv->conns[0]->size < srv->cfg->maxconns) {
                const int client = accept(srv->socket, NULL, NULL);
                if(client < 0) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) {
                                return SEL_ERR_OK;
                        } else {
                                SEL_THROW(SEL_ERR_SYS);
                        }
                }
                const int flags = fcntl(client, F_GETFL);
                if(flags < 0) {
                        SEL_THROW(SEL_ERR_SYS);
                } else if(fcntl(client, F_SETFL, O_NONBLOCK | flags) < 0) {
                        SEL_THROW(SEL_ERR_SYS);
                }
                *law_srv_conns_take(srv->conns[0]) = law_srv_conn_create(
                        client,
                        srv->cfg->stacklen, 
                        srv->cfg->guardlen);
        }
        return SEL_ERR_OK;
}

static sel_err_t law_srv_loop(struct law_srv *srv)
{
        while(srv->mode == LAW_SRV_RUNNING) {

                /* Call user's loop function. */
                SEL_TRY_QUIETLY(srv->cfg->loop(srv, srv->cfg->state));

                /** Accept new connections. */
                SEL_TRY_QUIETLY(law_srv_accept(srv));

                /* Dispatch coroutines. */
                SEL_TRY_QUIETLY(law_srv_dispatch(srv));

                /* Setup polling data for the server's socket. */
                struct pollfd *pfd = law_srv_lease(srv);
                pfd->fd = srv->socket;
                pfd->events = POLLIN;
                pfd->revents = 0;

                SEL_IO(poll(
                        srv->pfds[0]->array,
                        srv->pfds[0]->size,
                        srv->cfg->timeout));
        }
        return SEL_ERR_SYS;
}

sel_err_t law_srv_start(struct law_srv *server)
{
        if(server->mode == LAW_SRV_SUSPENDED) {
                server->mode = LAW_SRV_RUNNING;
                return law_srv_loop(server);
        } else {
                return LAW_ERR_MODE;
        }
}

sel_err_t law_srv_stop(struct law_srv *server)
{
        server->mode = LAW_SRV_SUSPENDED;
        return SEL_ERR_OK;
}