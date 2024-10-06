
#include "lawd/error.h"
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/time.h"
#include "lawd/pqueue.h"
#include "lawd/cqueue.h"
#include "lawd/log.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#ifndef LAW_INIT_TASKS_CAP
/** Initial task container capacity. */
#define LAW_INIT_TASKS_CAP 2
#endif

struct law_srv_cfg *law_srv_cfg_sanity()
{
        static struct law_srv_cfg cfg;
        cfg.protocol            = LAW_SRV_TCP;
        cfg.port                = 80;
        cfg.backlog             = 10;
        cfg.maxconns            = 100;  
        cfg.timeout             = 5000;
        cfg.stack_length        = 0x100000;
        cfg.stack_guard         = 0x1000;
        cfg.uid                 = 1000;
        cfg.gid                 = 1000;
        cfg.event_buffer        = 32;
        cfg.workers             = 1;
        cfg.data                = (struct law_data){ .u = { .u64 = 0 } };
        cfg.init                = NULL;
        cfg.tick                = NULL;
        cfg.accept              = NULL;
        cfg.errors              = stderr;
        return &cfg;
}

struct law_task {
        struct law_cor *callee;
        struct law_smem *stack;
        law_srv_call_t callback;
        struct law_data data;
        int mark;
        int mode;
};

struct law_tasks {
        struct law_task **array;
        size_t capacity;
        size_t size;
};

struct law_worker {
        struct law_server *server;
        struct law_cor *caller;
        struct law_pqueue *timers;
        struct law_tasks *ready;
        struct law_task *active;
        int epfd;
        int id;
        struct epoll_event *events;
};

struct law_server {
        struct law_srv_cfg cfg;
        int epfd;
        int mode;
        int alertfd;
        int socket;
        pthread_t *threads;
        struct law_cqueue *channel;
        struct law_worker **workers;
        struct epoll_event *events;
};

struct law_srv_poll_rec {
        struct law_task *task;
        sel_err_t signal;
};

enum law_srv_ctrl {
        LAW_SRV_SPAWNED         = 1,
        LAW_SRV_YIELDED         = 2,
        LAW_SRV_CREATED         = 3,
        LAW_SRV_RUNNING         = 4,
        LAW_SRV_STOPPED         = 5
};

struct law_tasks *law_tasks_create()
{
        struct law_tasks *tasks = malloc(sizeof(struct law_tasks));
        if(!tasks) {
                return NULL;
        }
        tasks->array = calloc(LAW_INIT_TASKS_CAP, sizeof(void*));
        if(!tasks->array) {
                free(tasks);
                return NULL;
        }
        tasks->capacity = LAW_INIT_TASKS_CAP;
        tasks->size = 0;
        return tasks;
}

void law_tasks_destroy(struct law_tasks *tasks)
{
        if(!tasks) {
                return;
        }
        free(tasks->array);
        free(tasks);
}

struct law_tasks *law_tasks_reset(struct law_tasks *tasks)
{
        SEL_ASSERT(tasks);
        memset(tasks->array, 0, sizeof(void*) * tasks->size);
        tasks->size = 0;
        return tasks;
}

struct law_tasks *law_tasks_grow(struct law_tasks *ps)
{
        const size_t old_cap = ps->capacity;
        const size_t new_cap = old_cap * 2;
        ps->array = realloc(ps->array, new_cap * sizeof(void*));
        if(!ps->array) {
                return NULL;
        }
        memset(ps->array + old_cap, 0, old_cap * sizeof(void*));
        ps->capacity = new_cap;
        return ps;
}

struct law_task **law_tasks_array(struct law_tasks *tasks) 
{
        return tasks->array;
}

size_t law_tasks_size(struct law_tasks *tasks) 
{
        return tasks->size;
}

struct law_tasks *law_tasks_add(
        struct law_tasks *tasks, 
        struct law_task *task)
{
        SEL_ASSERT(tasks);
        SEL_ASSERT(tasks->size <= tasks->capacity);
        if(tasks->capacity == tasks->size) {
                if(!law_tasks_grow(tasks)) {
                        return NULL;
                }
        }
        tasks->array[tasks->size++] = task;
        return tasks;
}

struct law_worker *law_worker_create(
        struct law_server *server,
        const int id)
{
        SEL_ASSERT(server);

        const int events_size = server->cfg.event_buffer;

        struct law_worker *w = malloc(sizeof(struct law_worker));
        if(!w) {
                return NULL;
        }

        w->caller = law_cor_create();
        if(!w->caller) {
                goto FREE_WORKER;
        }

        w->ready = law_tasks_create();
        if(!w->ready) {
                goto FREE_COR;
        }

        w->timers = law_pq_create();
        if(!w->timers) {
                goto FREE_READY;
        }

        w->events = malloc((size_t)events_size * sizeof(struct epoll_event));
        if(!w->events) {
                goto FREE_TIMERS;
        }

        w->server = server;
        w->id = id;
        w->active = NULL;
        w->epfd = 0;
        w->active = NULL;
        
        return w;

        FREE_TIMERS:
        law_pq_destroy(w->timers);

        FREE_READY:
        law_tasks_destroy(w->ready);

        FREE_COR:
        law_cor_destroy(w->caller);

        FREE_WORKER:
        free(w);

        return NULL;
}

void law_worker_destroy(struct law_worker *w)
{
        if(!w) {
                return;
        }
        free(w->events);
        law_pq_destroy(w->timers);
        law_tasks_destroy(w->ready);
        law_cor_destroy(w->caller);
        free(w);
}

struct law_server *law_srv_create(struct law_srv_cfg *cfg) 
{
        SEL_ASSERT(cfg);

        const int nthreads = cfg->workers;
        const int events_size = cfg->event_buffer;

        if(!nthreads || nthreads > 0x10000) {
                return NULL;
        }

        struct law_server *s = malloc(sizeof(struct law_server));
        if(!s) {
                return NULL;
        }

        s->cfg = *cfg;
        s->mode = LAW_SRV_CREATED;
        s->socket = 0;
        s->epfd = 0;
        s->alertfd = 0;

        s->channel = law_cq_create();
        if(!s->channel) {
                goto FREE_SERVER;
        }

        s->threads = calloc((size_t)nthreads, sizeof(pthread_t));
        if(!s->threads) {
                goto FREE_CHANNEL;
        }

        s->workers = calloc((size_t)nthreads, sizeof(struct law_worker*));
        if(!s->workers) {
                goto FREE_THREADFDS;
        }

        for(int n = 0; n < nthreads; ++n) {
                s->workers[n] = law_worker_create(s, n);
                if(!s->workers[n]) {
                        goto FREE_WORKERS;
                }
        }

        s->events = malloc((size_t)events_size * sizeof(struct epoll_event));
        if(!s->events) {
                goto FREE_WORKERS;
        }

        return s;

        FREE_WORKERS:
        for(int n = 0; n < nthreads; ++n) {
                law_worker_destroy(s->workers[n]);
        }
        free(s->workers);

        FREE_THREADFDS:
        free(s->threads);

        FREE_CHANNEL:
        law_cq_destroy(s->channel);

        FREE_SERVER:
        free(s);

        return NULL;
}

void law_srv_destroy(struct law_server *s) 
{       
        if(!s) {
                return;
        }
        free(s->events);
        for(unsigned int n = 0; n < s->cfg.workers; ++n) {
                law_worker_destroy(s->workers[n]);
        }
        free(s->workers);
        free(s->threads);
        law_cq_destroy(s->channel);
        free(s);
}

struct law_task *law_srv_active(struct law_worker *worker)
{
        return worker->active;
}

int law_srv_socket(struct law_server *server)
{
        return server->socket;
}

FILE *law_srv_errors(struct law_server *server)
{
        return server->cfg.errors;
}

struct law_task *law_task_create(
        law_srv_call_t callback,
        struct law_data *data,
        const size_t stack_length,
        const size_t stack_guard)
{
        struct law_task *task = malloc(sizeof(struct law_task));
        if(!task) {
                return NULL;
        }

        task->mode = LAW_SRV_SPAWNED;
        task->mark = 1;
        task->callback = callback;
        task->data = *data;
        
        task->callee = law_cor_create();
        if(!task->callee) {
                goto FREE_TASK;
        }

        task->stack = law_smem_create(stack_length, stack_guard);
        if(!task->stack) {
                goto FREE_CALLEE;
        }

        return task;

        FREE_CALLEE:
        free(task->callee);

        FREE_TASK:
        free(task);

        return NULL;
}

void law_task_destroy(struct law_task *task) 
{
        if(!task) {
                return;
        }
        law_smem_destroy(task->stack);
        law_cor_destroy(task->callee);
        free(task);
}

struct law_server *law_srv_server(struct law_worker *worker) 
{
        return worker->server;
}

sel_err_t law_srv_watch(struct law_worker *worker, const int sock)
{
        struct epoll_event ev = { .events = 0, .data.ptr = NULL };
        SEL_IO_QUIETLY(epoll_ctl(worker->epfd, EPOLL_CTL_ADD, sock, &ev));
        return LAW_ERR_OK;
}

sel_err_t law_srv_unwatch(struct law_worker *worker,const int sock)
{
        struct epoll_event ev = { .events = 0, .data.ptr = NULL };
        SEL_IO_QUIETLY(epoll_ctl(worker->epfd, EPOLL_CTL_DEL, sock, &ev));
        return LAW_ERR_OK;
}

sel_err_t law_srv_poll(
        struct law_worker *w,
        int64_t timeout,
        const int nevents, 
        struct law_event *events)
{
        struct law_pq_pair *pair;
        sel_err_t err;
        struct epoll_event ev;
        struct law_srv_poll_rec rec;
        rec.task = w->active;
        rec.signal = LAW_ERR_OK;

        for(int n = 0; n < nevents; ++n) {
                events[n].record = &rec;
                events[n].revents = 0;
        }
        for(int n = 0; n < nevents; ++n) {
                struct law_event *le = events + n;
                ev.events = le->events | EPOLLONESHOT;
                ev.data.ptr = le;
                if(epoll_ctl(w->epfd, EPOLL_CTL_MOD, le->fd, &ev) < 0) {
                        err = LAW_ERR_SYS;
                        goto UNDO_EPOLL_CTL;
                }
        }

        pair = law_pq_insert(w->timers, law_time_millis() + timeout, &rec);
        if(!pair) {
                err = LAW_ERR_OOM;
                goto UNDO_EPOLL_CTL;
        } 

        law_cor_yield(w->caller, rec.task->callee, LAW_SRV_YIELDED);

        if(rec.signal != LAW_ERR_TTL) {
                /* Pair should still be in the timer set. */
                pair->value = NULL;
        } 

        ev = (struct epoll_event){ .events = 0, .data.ptr = NULL };
        for(int n = 0; n < nevents; ++n) {
                struct law_event *le = events + n;
                if(!le->revents) {
                        epoll_ctl(w->epfd, EPOLL_CTL_MOD, le->fd, &ev);
                }
        }
        
        return rec.signal;

        UNDO_EPOLL_CTL:
        ev = (struct epoll_event){ .events = 0, .data.ptr = NULL };
        for(int n = 0; n < nevents; ++n) {
                epoll_ctl(w->epfd, EPOLL_CTL_MOD, events[n].fd, &ev);
        }
        return err;
}

sel_err_t law_srv_spawn(
        struct law_server *server,
        law_srv_call_t callback,
        struct law_data *data)
{
        struct law_task *task = law_task_create(
                callback,
                data,
                server->cfg.stack_length,
                server->cfg.stack_guard);
        if(!task) {
                return LAW_ERR_OOM;
        } else if(!law_cq_enq(server->channel, task)) {
                law_task_destroy(task);
                return LAW_ERR_OOM;
        } else {
                return LAW_ERR_OK;
        }
}

int law_worker_trampoline(
        struct law_cor *init_env, 
        struct law_cor *cor_env, 
        void *state)
{
        struct law_worker *worker = state;
        struct law_task *task = worker->active;
        return task->callback(worker, task->data);
}

static sel_err_t law_worker_dispatch(struct law_worker *worker)
{
        struct law_tasks *ready = worker->ready;

        for(size_t n = 0; n < ready->size; ++n) {
                ready->array[n]->mark = 1;
        }

        for(size_t n = 0; n < ready->size; ++n) {

                struct law_task *task = ready->array[n];
                if(!task->mark) {
                        continue;
                }
                
                const int mode = task->mode;
                sel_err_t signal;

                if(mode == LAW_SRV_SPAWNED) {
                        
                        signal = LAW_ERR_OK;
                        worker->active = task;
                        signal = law_cor_call(
                                worker->caller, 
                                task->callee, 
                                task->stack,
                                law_worker_trampoline,
                                worker);
                } else if(mode == LAW_SRV_YIELDED) {
                        signal = LAW_ERR_OK;
                        worker->active = task;
                        signal = law_cor_resume(
                                worker->caller,
                                task->callee,
                                LAW_ERR_OK);
                } else {
                        return SEL_ABORT();
                }

                if(signal == LAW_SRV_YIELDED) {
                        /* Task should be in the timer set. */
                        task->mode = LAW_SRV_YIELDED;
                        task->mark = 0;
                } else if(signal == LAW_ERR_OK) {
                        law_task_destroy(task);
                } else {
                        law_task_destroy(task);
                        return signal;
                }
        }

        law_tasks_reset(ready);

        return LAW_ERR_OK;
}

static sel_err_t law_worker_tick(struct law_worker *worker)
{
        struct law_server *server = worker->server;
        FILE *errs = server->cfg.errors;
        struct law_cqueue *channel = server->channel;
        struct law_pqueue *timers = worker->timers;
        struct epoll_event *events = worker->events;
        struct law_tasks *ready = worker->ready;

        int64_t timeout = server->cfg.timeout;
        int64_t min_timer, now;
        int cbuf = 0;
        struct law_srv_poll_rec *rec;
        struct law_task *task;

        if(!law_cq_size(channel)) {
                if(law_pq_peek(timers, &min_timer, NULL)) {
                        now = law_time_millis();
                        if(min_timer <= now) {
                                timeout = 0;
                        } else {
                                const int64_t wake = min_timer - now;
                                timeout = timeout < wake ? timeout : wake;
                        }
                }
        } else {
                timeout = 0;
        }

        const int event_cnt = epoll_wait(
                worker->epfd, 
                events, 
                server->cfg.event_buffer, 
                (int)timeout);
        if(event_cnt < 0) {
                law_log_error(errs, "lawd", "epoll_wait", strerror(errno));
                return SEL_FREPORT(errs, LAW_ERR_SYS);
        }

        for(int x = 0; x < event_cnt; ++x) {
                void *ptr = events[x].data.ptr;
                if(ptr == &server->alertfd) {
                        read(server->alertfd, &cbuf, 1);
                } else {
                        struct law_event *ev = ptr;
                        rec = ev->record;
                        ev->revents = events[x].events;
                        law_tasks_add(ready, rec->task);
                }
        }

        now = law_time_millis();
        while(law_pq_peek(timers, &min_timer, (void**)&rec)) {
                if(min_timer <= now) {
                        law_pq_pop(timers);
                        if(!rec) {
                                continue;
                        }
                        rec->signal = LAW_ERR_TTL;
                        if(!law_tasks_add(ready, rec->task)) {
                                return SEL_FREPORT(errs, LAW_ERR_OOM);
                        }
                } else {
                        break;
                }
        }

        int chunk = (int)law_cq_size(channel) / server->cfg.workers;
        chunk = chunk ? chunk : 1;
        for(int x = 0; law_cq_deq(channel, (void**)&task); ++x) {
                if(!law_tasks_add(ready, task)) {
                        return SEL_FREPORT(errs, LAW_ERR_OOM);
                } 
                if(x == chunk) {
                        break;
                }
        }

        return law_worker_dispatch(worker);
}

static sel_err_t law_worker_spin(struct law_worker *w)
{
        struct law_server *s = w->server;
        SEL_TRY_QUIETLY(s->cfg.init(w, s->cfg.data));
        while(  s->mode == LAW_SRV_RUNNING 
                || law_pq_size(w->timers)
                || law_cq_size(s->channel)) 
        {
                SEL_TRY_QUIETLY(s->cfg.tick(w, s->cfg.data));
                SEL_TRY_QUIETLY(law_worker_tick(w));
        }
        return LAW_ERR_OK;
}

static sel_err_t law_worker_run(struct law_worker *worker)
{
        struct law_server *s = worker->server;
        FILE *errs = s->cfg.errors;

        worker->epfd = epoll_create(s->cfg.event_buffer);
        if(worker->epfd < 0) {
                law_log_error(errs, "lawd", "epoll_create", strerror(errno));
                return SEL_FREPORT(errs, LAW_ERR_SYS);
        }

        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = &s->alertfd };
        if(epoll_ctl(worker->epfd, EPOLL_CTL_ADD, s->alertfd, &ev) < 0) {
                close(worker->epfd);
                law_log_error(errs, "lawd", "epoll_ctl", strerror(errno));
                return SEL_FREPORT(errs, LAW_ERR_SYS);
        }

        sel_err_t err = law_worker_spin(worker);

        epoll_ctl(worker->epfd, EPOLL_CTL_DEL, s->alertfd, NULL);
        close(worker->epfd);

        return err;
}

void *law_worker_thread(void *state) 
{
        struct law_worker *worker = state;
        law_worker_run(worker);
        return NULL;
}

static sel_err_t law_server_trampoline(
        struct law_worker *worker, 
        struct law_data data)
{
        struct law_server *server = worker->server;
        int socket = data.u.fd;
        return server->cfg.accept(worker, socket, server->cfg.data);
}

static sel_err_t law_server_accept(struct law_server *server) 
{
        FILE *errs = server->cfg.errors;
        /* Oops!  Should check for maxconns probably. */
        for(;;) {
                const int client = accept(server->socket, NULL, NULL);
                if(client < 0) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                        } else {
                                const char *str = strerror(errno);
                                law_log_error(errs, "lawd", "accept", str);
                                return SEL_FREPORT(errs, SEL_ERR_SYS);
                        }
                }
                const int flags = fcntl(client, F_GETFL);
                if(flags < 0) {
                        close(client);
                        law_log_error(errs, "lawd", "fcntl", strerror(errno));
                        return SEL_FREPORT(errs, SEL_ERR_SYS);
                } else if(fcntl(client, F_SETFL, O_NONBLOCK | flags) < 0) {
                        close(client);
                        law_log_error(errs, "lawd", "fcntl", strerror(errno));
                        return SEL_FREPORT(errs, SEL_ERR_SYS);
                }
                struct law_data data = { .u.fd = client };
                SEL_TRY_QUIETLY(law_srv_spawn(
                        server, 
                        law_server_trampoline, 
                        &data));
        }
        return LAW_ERR_OK;
}

static sel_err_t law_server_spin(struct law_server *server, int pipe)
{
        FILE *errs = server->cfg.errors;
        char c = 0;
        while(server->mode == LAW_SRV_RUNNING) {
                SEL_TRY_QUIETLY(law_server_accept(server));
                if(write(pipe, &c, 1) < 0) {
                        law_log_error(errs, "lawd", "write", strerror(errno));
                        return SEL_FREPORT(errs, LAW_ERR_SYS);
                }
                if(epoll_wait(
                        server->epfd, 
                        server->events, 
                        server->cfg.event_buffer, 
                        server->cfg.timeout * 2) < 0) 
                {
                        const char *str = strerror(errno);
                        law_log_error(errs, "lawd", "epoll_wait", str);
                        return SEL_FREPORT(errs, LAW_ERR_SYS);
                }
        }
        return LAW_ERR_OK;
}

static sel_err_t law_server_run_threads(struct law_server *s, const int pipe)
{
        FILE *errs = s->cfg.errors;
        struct law_worker **workers = s->workers;
        pthread_t *threads = s->threads;
        const int nthreads = s->cfg.workers;
        sel_err_t err;
        int n = 0;
        for(; n < nthreads; ++n) {
                struct law_worker *worker = workers[n];
                SEL_ASSERT(worker);
                worker->id = n;
                worker->server = s;
                if(pthread_create(
                        threads + n,
                        NULL, 
                        law_worker_thread, 
                        worker) < 0) 
                {
                        const char *str = strerror(errno);
                        law_log_error(errs, "lawd", "pthread_create", str);
                        err = SEL_FREPORT(errs, LAW_ERR_SYS);
                        law_srv_stop(s);
                        goto JOIN_THREADS;
                }
        }
        err = law_server_spin(s, pipe);
        JOIN_THREADS:
        for(int m = 0; m < n; ++m) {
                pthread_join(threads[m], NULL);
        }
        return err;
}

static sel_err_t law_server_run_epoll(struct law_server *s, const int pipe)
{
        FILE *errs = s->cfg.errors;
        struct epoll_event ev;
        sel_err_t err;
        s->epfd = epoll_create(2);
        if(s->epfd < 0) {
                law_log_error(errs, "lawd", "epoll_create", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        }
        ev.data.ptr = &s->socket;
        ev.events = EPOLLIN | EPOLLERR;
        if(epoll_ctl(s->epfd, EPOLL_CTL_ADD, s->socket, &ev) < 0) {
                law_log_error(errs, "lawd", "epoll_ctl", strerror(errno));
                err = SEL_FREPORT(errs, SEL_ERR_SYS);
        } else {
                err = law_server_run_threads(s, pipe);
                epoll_ctl(s->epfd, EPOLL_CTL_DEL, s->socket, NULL);
        }
        close(s->epfd);
        return err;
}

static sel_err_t law_server_run(struct law_server *s)
{
        FILE *errs = s->cfg.errors;
        int pipefd[2];
        sel_err_t err;
        int flags;
        if(pipe(pipefd) < 0) {
                law_log_error(errs, "lawd", "pipe", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        }
        flags = fcntl(s->alertfd, F_GETFL);
        if(flags < 0) {
                law_log_error(errs, "lawd", "fcntl", strerror(errno));
                err = SEL_FREPORT(errs, SEL_ERR_SYS);
                goto CLOSE_PIPE;
        } else if(fcntl(s->alertfd, F_SETFL, O_NONBLOCK | flags) < 0) {
                law_log_error(errs, "lawd", "fcntl", strerror(errno));
                err = SEL_FREPORT(errs, SEL_ERR_SYS);
                goto CLOSE_PIPE;
        }
        s->alertfd = pipefd[0];
        err = law_server_run_epoll(s, pipefd[1]);
        CLOSE_PIPE:
        close(pipefd[1]);
        close(pipefd[0]);
        return err;
}

sel_err_t law_srv_start(struct law_server *server)
{
        if(server->mode != LAW_SRV_CREATED) {
                return LAW_ERR_MODE;
        }
        server->mode = LAW_SRV_RUNNING;
        return law_server_run(server);
}

sel_err_t law_srv_stop(struct law_server *server)
{
        server->mode = LAW_SRV_STOPPED;
        return LAW_ERR_OK;
}

sel_err_t law_srv_open(struct law_server *s)
{
        FILE *errs = s->cfg.errors;

        struct sockaddr_storage addr;
        memset(&addr, 0, sizeof(struct sockaddr_storage));
        int domain, type;
        unsigned int addrlen;
        struct sockaddr_in *in;
        struct sockaddr_in6 *in6;

        switch(s->cfg.protocol) {
                case LAW_SRV_TCP: 
                        domain = AF_INET;
                        type = SOCK_STREAM;
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons(s->cfg.port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                        break;
                case LAW_SRV_TCP6:
                        domain = AF_INET6;
                        type = SOCK_STREAM;
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons(s->cfg.port);
                        in6->sin6_addr = in6addr_any;
                        break;
                case LAW_SRV_UDP:
                        domain = AF_INET;
                        type = SOCK_DGRAM;
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons(s->cfg.port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                        break;
                case LAW_SRV_UDP6:
                        domain = AF_INET6;
                        type = SOCK_DGRAM;
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons(s->cfg.port);
                        in6->sin6_addr = in6addr_any;
                        break;
                default: 
                        return SEL_ABORT();
        }

        const int fd = socket(domain, type, 0);
        if(fd < 0) { 
                law_log_error(errs, "lawd", "socket", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        }
        
        const int flags = fcntl(fd, F_GETFL);
        if(flags < 0) {
                close(fd);
                law_log_error(errs, "lawd", "fcntl", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        } else if(fcntl(fd, F_SETFL, O_NONBLOCK | flags) < 0) {
                close(fd);
                law_log_error(errs, "lawd", "fcntl", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        }

        if(bind(fd, (struct sockaddr*)&addr, addrlen) < 0) {
                close(fd);
                law_log_error(errs, "lawd", "bind", strerror(errno));
                return SEL_REPORT(SEL_ERR_SYS);
        }
        if(listen(fd, s->cfg.backlog) < 0) { 
                close(fd);
                law_log_error(errs, "lawd", "listen", strerror(errno));
                return SEL_REPORT(SEL_ERR_SYS);
        }

        if(setgid(s->cfg.gid) < 0) { 
                close(fd);
                law_log_error(errs, "lawd", "getgid", strerror(errno));
                return SEL_REPORT(SEL_ERR_SYS);
        }
        if(setuid(s->cfg.uid) < 0) {
                close(fd);
                law_log_error(errs, "lawd", "setuid", strerror(errno));
                return SEL_REPORT(SEL_ERR_SYS);
        }

        s->socket = fd;

        return SEL_ERR_OK;
}

sel_err_t law_srv_close(struct law_server *server)
{
        SEL_IO_QUIETLY(close(server->socket));
        return SEL_ERR_OK;
}