
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/time.h"
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>

enum law_srv_mode {
        LAW_SRV_READY           = 0,            /** Ready */
        LAW_SRV_SUSPENDED       = 1,            /** Suspended */
        LAW_SRV_RUNNING         = 2,            /** Running */
        LAW_SRV_CLOSED          = 3,            /** Closed */
        LAW_SRV_YIELDING        = 4,            /** Coroutine Yielded */
        LAW_SRV_AWAITING        = 5             /** Coroutine Awaited */
};

struct law_srv_conn {                           /** Network Connection */
        enum law_srv_mode mode;                 /** Connection Mode */
        int socket;                             /** Socket Descriptor */
        struct law_cor *callee;                 /** Coroutine Environment */
        struct law_smem *stack;                 /** Coroutine Stack */
        law_srv_call_t callback;                /** Coroutine callback */
        void *state;                            /** Coroutine user-state */
        struct pollfd *pollfd;                  /** Polling Data */
        int64_t expires;                        /** Await Expiration */
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
        const size_t guardlen,
        law_srv_call_t callback,
        void *state)
{
        struct law_srv_conn *conn = malloc(sizeof(struct law_srv_conn));
        conn->callee = law_cor_create();
        conn->stack = law_smem_create(stacklen, guardlen);
        conn->mode = LAW_SRV_READY;
        conn->socket = socket;
        conn->callback = callback;
        conn->state = state;
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

sel_err_t law_srv_yield(struct law_srv *server)
{
        return law_cor_yield(
                server->caller, 
                server->target->callee, 
                LAW_SRV_YIELDING);
}

struct pollfd *law_srv_lease(struct law_srv *s)
{
        return law_srv_pfds_take(s->pfds[0]);
}

sel_err_t law_srv_spawn(
        struct law_srv *srv,
        law_srv_call_t callback,
        const int socket,
        void *state)
{
        *law_srv_conns_take(srv->conns[0]) = law_srv_conn_create(
                socket,
                srv->cfg->stack_length, 
                srv->cfg->stack_guard,
                callback,
                state);
        return SEL_ERR_OK;
}

sel_err_t law_srv_wait(
        struct law_srv *server,
        const int socket,
        const int events,
        int *revents,
        int64_t timeout)
{
        struct law_srv_conn *conn = server->target;

        conn->pollfd = law_srv_lease(server);
        conn->pollfd->fd = socket;
        conn->pollfd->events = (short)events;
        conn->pollfd->revents = 0;
      
        conn->expires = law_time_millis() + timeout;

        sel_err_t error = law_cor_yield(
                server->caller, 
                conn->callee, 
                LAW_SRV_AWAITING);

        if(revents) {
                *revents = conn->pollfd->revents;
        }
      
        return error;
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
        struct law_srv *server = state;
        struct law_srv_conn *conn = server->target;
        return conn->callback(server, conn->socket, conn->state);
}

static sel_err_t law_srv_dispatch_await(
        struct law_srv *srv,
        struct law_srv_conn *conn,
        const int64_t time)
{
        if(conn->pollfd->revents) {
                conn->mode = LAW_SRV_RUNNING;
                return law_cor_resume(srv->caller, conn->callee, LAW_ERR_OK);
        } 

        if(time > conn->expires) {
                conn->mode = LAW_SRV_RUNNING;
                return law_cor_resume(srv->caller, conn->callee, LAW_ERR_TTL);
        }

        struct pollfd *pollfd = law_srv_lease(srv);
        memcpy(pollfd, conn->pollfd, sizeof(struct pollfd));
        conn->pollfd = pollfd;

        return LAW_SRV_AWAITING;
}

static sel_err_t law_srv_dispatch(struct law_srv *srv)
{
        law_srv_conns_swap(srv);
        law_srv_conns_reset(srv->conns[0]);

        law_srv_pfds_swap(srv);
        law_srv_pfds_reset(srv->pfds[0]);

        struct law_srv_conns *conns1 = srv->conns[1];

        int64_t time = law_time_millis();

        for(size_t n = 0; n < conns1->size; ++n) {
                struct law_srv_conn *conn = conns1->array[n];
                srv->target = conn;
                sel_err_t err;
                switch(conn->mode) {
                        case LAW_SRV_READY:
                                conn->mode = LAW_SRV_RUNNING;
                                err = law_cor_call(
                                        srv->caller,
                                        conn->callee,
                                        conn->stack,
                                        law_srv_trampoline,
                                        srv); 
                                break;
                        case LAW_SRV_YIELDING:
                                conn->mode = LAW_SRV_RUNNING;
                                err = law_cor_resume(
                                        srv->caller,
                                        conn->callee,
                                        LAW_ERR_OK);
                                break;
                        case LAW_SRV_AWAITING:
                                err = law_srv_dispatch_await(
                                        srv,
                                        conn,
                                        time);
                                break;
                        default: SEL_ABORT();
                }
                switch(err) {
                        case SEL_ERR_OK:
                                law_srv_conn_destroy(conn);
                                break;
                        case LAW_SRV_YIELDING:
                        case LAW_SRV_AWAITING:
                                conn->mode = err;
                                *law_srv_conns_take(srv->conns[0]) = conn;
                                break;
                        default: 
                                law_srv_conn_destroy(conn);
                                return err;
                }
        }

        return SEL_ERR_OK;
}

static sel_err_t law_srv_accept(struct law_srv *srv)
{
        while(srv->conns[0]->size <= srv->cfg->maxconns) {

                /* Accept new client connections. */
                const int client = accept(srv->socket, NULL, NULL);

                if(client < 0) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) {
                                return SEL_ERR_OK;
                        } else {
                                SEL_THROW(SEL_ERR_SYS);
                        }
                }

                /* Set socket to non-blocking mode. */
                const int flags = fcntl(client, F_GETFL);
                if(flags < 0) {
                        SEL_THROW(SEL_ERR_SYS);
                } else if(fcntl(client, F_SETFL, O_NONBLOCK | flags) < 0) {
                        SEL_THROW(SEL_ERR_SYS);
                }

                /* Spawn a new coroutine. */
                SEL_TRY_QUIETLY(law_srv_spawn(
                        srv, 
                        srv->cfg->accept, 
                        client, 
                        srv->cfg->state));
        }
        return SEL_ERR_OK;
}

static sel_err_t law_srv_loop(struct law_srv *srv)
{
        while(srv->mode == LAW_SRV_RUNNING) {

                /* Call user's loop function. */
                SEL_TRY_QUIETLY(srv->cfg->tick(srv, srv->cfg->state));

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
        return SEL_ERR_OK;
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