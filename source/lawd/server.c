
/* See man getaddrinfo */
#define _POSIX_C_SOURCE 200112L

#include "lawd/error.h"
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/time.h"
#include "lawd/pqueue.h"
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
#include <stdatomic.h>
#include <time.h>
#include <math.h>

#ifndef LAW_INIT_TASKS_CAP
/** Initial task container capacity. */
#define LAW_INIT_TASKS_CAP 2
#endif

struct law_srv_cfg law_srv_sanity()
{
        static struct law_srv_cfg cfg;
        cfg.protocol            = LAW_SRV_TCP;
        cfg.port                = 80;
        cfg.backlog             = 10;
        cfg.maxconns            = 100;  
        cfg.worker_timeout      = 5000;
        cfg.server_timeout      = 10000;
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
        return cfg;
}

enum law_srv_mode {
        LAW_SRV_SPAWNED         = 1,
        LAW_SRV_SUSPENDED       = 2,
        LAW_SRV_CREATED         = 3,
        LAW_SRV_RUNNING         = 4,
        LAW_SRV_STOPPING        = 5,
        LAW_SRV_STOPPED         = 6,
        LAW_SRV_ENQUEUED        = 7,
        LAW_SRV_PROCESSING      = 8
};

struct law_task {
        int mode;
        int mark;
        int resched;
        enum law_srv_sig signal;
        pthread_mutex_t lock;
        struct law_worker *worker;
        struct law_cor *callee;
        struct law_smem *stack;
        law_srv_call_t callback;
        struct law_data data;
};

struct law_tasks {
        struct law_task **array;
        size_t capacity;
        size_t size;
};

struct law_pipe {
        int fds[2];
};

struct law_worker {
        int id;
        int epfd;
        struct law_pipe channel;
        struct law_server *server;
        struct law_cor *caller;
        struct law_pqueue *timers;
        struct law_tasks *ready;
        struct law_task *active;
        struct epoll_event *events;
};

struct law_server {
        struct law_srv_cfg cfg;
        volatile int mode;
        volatile int ntasks;
        int epfd;
        int socket;
        int spawn_index;
        pthread_t *threads;
        struct law_worker **workers;
        struct epoll_event *events;
};

struct law_tasks *law_tasks_create()
{
        struct law_tasks *tasks = calloc(1, sizeof(struct law_tasks));
        if(!tasks) 
                goto FAILURE;
        tasks->array = calloc(LAW_INIT_TASKS_CAP, sizeof(void*));
        if(!tasks->array) 
                goto FREE_TASKS;
        tasks->capacity = LAW_INIT_TASKS_CAP;
        return tasks;
        FREE_TASKS:
        free(tasks);
        FAILURE:
        return NULL;
}

void law_tasks_destroy(struct law_tasks *tasks)
{
        if(!tasks) 
                return;
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
        void **addr = realloc(ps->array, new_cap * sizeof(void*));
        if(!addr) return NULL;
        ps->array = memset(addr + old_cap, 0, old_cap * sizeof(void*));
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
        struct law_task *task,
        enum law_srv_sig signal)
{
        SEL_ASSERT(tasks->size <= tasks->capacity);
        if(task->mark) {
                return tasks;
        } else if(tasks->capacity == tasks->size) {
                if(!law_tasks_grow(tasks)) 
                        return NULL;
        }
        task->signal = signal;
        tasks->array[tasks->size++] = task;
        task->mark = 1;
        return tasks;
}

static void law_pipe_close(struct law_pipe *p)
{
        close(p->fds[0]);
        close(p->fds[1]);
}

static sel_err_t law_pipe_open(struct law_pipe *p)
{
        if(pipe(p->fds) < 0) return LAW_ERR_SYS;
        int flags = fcntl(p->fds[0], F_GETFL);
        SEL_TEST(flags != -1);
        SEL_TEST(fcntl(p->fds[0], F_SETFL, O_NONBLOCK | flags) != -1);
        SEL_TEST((flags = fcntl(p->fds[1], F_GETFL)) != -1);
        SEL_TEST(fcntl(p->fds[1], F_SETFL, O_NONBLOCK | flags) != -1);
        return LAW_ERR_OK;
}

static sel_err_t law_enqueue_task(
        struct law_worker *worker, 
        struct law_task *task)
{
        const int writer = worker->channel.fds[1];
        task->resched = 0;
        const ssize_t written = write(writer, &task, sizeof(void*));
        if(written < 0) {
                if(errno == EWOULDBLOCK || errno == EAGAIN)
                        return LAW_ERR_WNTW;
                else
                        SEL_HALT();
        }
        SEL_TEST(written == sizeof(void*));
        return LAW_ERR_OK;
}

struct law_worker *law_worker_create(
        struct law_server *server,
        const int id)
{
        SEL_ASSERT(server);
        const int nevents = server->cfg.event_buffer;
        struct law_worker *wrk = calloc(1, sizeof(struct law_worker));
        if(!wrk) 
                goto FAILURE;
        wrk->caller = law_cor_create();
        if(!wrk->caller) 
                goto FREE_WORKER;
        wrk->ready = law_tasks_create();
        if(!wrk->ready) 
                goto FREE_CALLER;
        wrk->timers = law_pq_create();
        if(!wrk->timers) 
                goto FREE_READY;
        wrk->events = calloc((size_t)nevents, sizeof(struct epoll_event));
        if(!wrk->events) 
                goto FREE_TIMERS;
        wrk->server = server;
        wrk->id = id;
        return wrk;
        FREE_TIMERS:
        law_pq_destroy(wrk->timers);
        FREE_READY:
        law_tasks_destroy(wrk->ready);
        FREE_CALLER:
        law_cor_destroy(wrk->caller);
        FREE_WORKER:
        free(wrk);
        FAILURE:
        return NULL;
}

void law_worker_destroy(struct law_worker *w)
{
        if(!w) 
                return;
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
        const int nevents = cfg->event_buffer;
        SEL_ASSERT(0 < nthreads && nthreads < 0x1000);
        SEL_ASSERT(0 < nevents && nevents < 0x40000);
        int n = 0;
        struct law_server *srv = calloc(1, sizeof(struct law_server));
        if(!srv) 
                goto FAILURE;
        srv->cfg = *cfg;
        srv->mode = LAW_SRV_CREATED;
        srv->threads = calloc((size_t)nthreads, sizeof(pthread_t));
        if(!srv->threads) 
                goto FREE_SERVER;
        srv->workers = calloc((size_t)nthreads, sizeof(struct law_worker*));
        if(!srv->workers) 
                goto FREE_THREADS;
        srv->events = calloc((size_t)nevents, sizeof(struct epoll_event));
        if(!srv->events) 
                goto FREE_WORKERS;
        for(; n < nthreads; ++n) {
                srv->workers[n] = law_worker_create(srv, n);
                if(!srv->workers[n]) 
                        goto FREE_ALL;
        }
        return srv;
        FREE_ALL:
        for(int m = 0; m < n; ++m) {
                law_worker_destroy(srv->workers[m]);
        }
        free(srv->events);
        FREE_WORKERS:
        free(srv->workers);
        FREE_THREADS:
        free(srv->threads);
        FREE_SERVER:
        free(srv);
        FAILURE:
        return NULL;
}

void law_srv_destroy(struct law_server *srv) 
{       
        if(!srv) 
                return;
        for(int n = 0; n < srv->cfg.workers; ++n) {
                law_worker_destroy(srv->workers[n]);
        }
        free(srv->events);
        free(srv->workers);
        free(srv->threads);
        free(srv);
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
        SEL_ASSERT(callback && data && stack_length);
        struct law_task *task = calloc(1, sizeof(struct law_task));
        if(!task) 
                goto FAILURE;
        task->mode = LAW_SRV_SPAWNED;
        task->callback = callback;
        task->data = *data;
        task->callee = law_cor_create();
        if(!task->callee) 
                goto FREE_TASK;
        task->stack = law_smem_create(stack_length, stack_guard);
        if(!task->stack) 
                goto FREE_COR;
        return task;
        FREE_COR:
        law_cor_destroy(task->callee);
        FREE_TASK:
        free(task);
        FAILURE:
        return NULL;
}

void law_task_destroy(struct law_task *task) 
{
        if(!task) 
                return;
        law_smem_destroy(task->stack);
        law_cor_destroy(task->callee);
        free(task);
}

struct law_server *law_srv_server(struct law_worker *worker) 
{
        return worker->server;
}

sel_err_t law_srv_add(
        struct law_worker *w, 
        const int fd,
        struct law_event *law_ev)
{
        SEL_ASSERT(w && w->active && law_ev);
        struct epoll_event ep_ev;
        memset(&ep_ev, 0, sizeof(struct epoll_event));
        law_ev->task = w->active;
        law_ev->revents = 0;
        ep_ev.events = law_ev->events;
        ep_ev.data.ptr = (void*)law_ev;
        if(epoll_ctl(w->epfd, EPOLL_CTL_ADD, fd, &ep_ev) < 0) 
                return LAW_ERR_SYS;
        return LAW_ERR_OK;
}

sel_err_t law_srv_mod(
        struct law_worker *w, 
        const int fd,
        struct law_event *law_ev)
{
        SEL_ASSERT(w && w->active && law_ev);
        struct epoll_event ep_ev;
        memset(&ep_ev, 0, sizeof(struct epoll_event));
        law_ev->task = w->active;
        law_ev->revents = 0;
        ep_ev.events = law_ev->events;
        ep_ev.data.ptr = law_ev;
        if(epoll_ctl(w->epfd, EPOLL_CTL_MOD, fd, &ep_ev) == -1) 
                return LAW_ERR_SYS;
        return LAW_ERR_OK;
}

sel_err_t law_srv_del(struct law_worker *w, const int fd)
{
        struct epoll_event ep_ev;
        memset(&ep_ev, 0, sizeof(struct epoll_event));
        if(epoll_ctl(w->epfd, EPOLL_CTL_DEL, fd, &ep_ev) == -1) 
                return LAW_ERR_SYS;
        return LAW_ERR_OK;
}

sel_err_t law_srv_sleep(struct law_worker *worker, const int64_t timeout)
{
        struct law_task *task = worker->active;
        const int64_t wake = law_time_millis() + timeout;
        struct law_pq_pair *pair = law_pq_insert(worker->timers, wake, task);
        if(!pair) return LAW_ERR_OOM;
        task->signal = task->mark = 0;
        law_cor_yield(worker->caller, task->callee, LAW_SRV_SUSPENDED);
        if(task->signal != LAW_SRV_SIGTTL) pair->value = NULL;
        return task->signal;
}

sel_err_t law_srv_notify(struct law_task *task)
{
        SEL_ASSERT(task);
        struct law_worker *worker = task->worker;
        sel_err_t err = -1;
        (void)pthread_mutex_lock(&task->lock);
        switch(task->mode) {
                case LAW_SRV_SUSPENDED:
                        err = law_enqueue_task(worker, task);
                        if(err == LAW_ERR_OK) 
                                task->mode = LAW_SRV_ENQUEUED;
                        else if(err != LAW_ERR_WNTW) 
                                SEL_HALT();
                        break;
                case LAW_SRV_PROCESSING:
                        task->resched = 1;
                case LAW_SRV_ENQUEUED:
                case LAW_SRV_SPAWNED:
                        err = LAW_ERR_OK;
                        break;
                default: 
                        SEL_HALT();
        }
        (void)pthread_mutex_unlock(&task->lock);
        return err;
}

sel_err_t law_srv_spawn(
        struct law_server *server,
        law_srv_call_t callback,
        struct law_data *data)
{
        SEL_ASSERT(server);
        struct law_task *task = law_task_create(
                callback,
                data,
                server->cfg.stack_length,
                server->cfg.stack_guard);
        if(!task) 
                return LAW_ERR_OOM;
        SEL_ASSERT(task->mode == LAW_SRV_SPAWNED);
        atomic_fetch_add(&server->ntasks, 1);
        for(int I = 0; I < 8; ++I) {
                const int index = server->spawn_index;
                server->spawn_index = (index + 1) % server->cfg.workers;
                struct law_worker *worker = server->workers[index];
                const sel_err_t err = law_enqueue_task(worker, task);
                if(err == LAW_ERR_OK) {
                        task->worker = worker;
                        return LAW_ERR_OK;
                } else if(err == LAW_ERR_WNTW) {
                        struct timespec ts;
                        memset(&ts, 0, sizeof(struct timespec));
                        ts.tv_nsec = 1000000 * 50 * (1 << I);
                        SEL_TEST(nanosleep(&ts, NULL) != -1);
                } else {
                        SEL_HALT();
                }
        }
        atomic_fetch_sub(&server->ntasks, 1);
        law_task_destroy(task);
        return LAW_ERR_WNTW;
}

static int law_worker_trampoline(
        struct law_cor *init_env, 
        struct law_cor *cor_env, 
        void *state)
{
        struct law_worker *worker = state;
        struct law_task *task = worker->active;
        return task->callback(worker, task->data);
}

static sel_err_t law_worker_dispatch(struct law_worker *w)
{
        struct law_tasks *ready = w->ready;
        struct law_server *server = w->server;
        const size_t size = ready->size;
        SEL_ASSERT(0 <= size && size <= ready->capacity);
        for(size_t n = 0; n < size; ++n) {
                SEL_ASSERT(ready->array[n]);
                struct law_task *task = ready->array[n];
                w->active = task;
                task->mark = 0;
                (void)pthread_mutex_lock(&task->lock);
                const int saved_mode = task->mode;
                task->mode = LAW_SRV_PROCESSING;
                (void)pthread_mutex_unlock(&task->lock);
                const sel_err_t sig = saved_mode == LAW_SRV_SPAWNED ?
                        law_cor_call(
                                w->caller, 
                                task->callee, 
                                task->stack,
                                law_worker_trampoline,
                                w) :
                        law_cor_resume(
                                w->caller,
                                task->callee,
                                LAW_ERR_OK);
                if(sig == LAW_SRV_SUSPENDED) {
                        (void)pthread_mutex_lock(&task->lock);
                        if(!task->resched) 
                                task->mode = LAW_SRV_SUSPENDED;
                        else if(law_enqueue_task(w, task) == LAW_ERR_OK)
                                task->mode = LAW_SRV_ENQUEUED;
                        else
                                task->mode = LAW_SRV_SUSPENDED;
                        (void)pthread_mutex_unlock(&task->lock);
                } else {
                        law_task_destroy(task);
                        atomic_fetch_sub(&server->ntasks, 1);
                        SEL_ASSERT(atomic_load(&server->ntasks) >= 0);
                }        
        }
        law_tasks_reset(ready);
        return LAW_ERR_OK;
}

static sel_err_t law_worker_tick(struct law_worker *worker)
{
        struct law_server *server = worker->server;
        struct law_pqueue *timers = worker->timers;
        struct epoll_event *events = worker->events;
        struct law_tasks *ready = worker->ready;
        const int reader = worker->channel.fds[0];

        int64_t timeout = server->cfg.worker_timeout;
        int64_t min_timer, now = 0;
        struct law_task *task = NULL;
       
        if(law_pq_peek(timers, &min_timer, NULL)) {
                now = law_time_millis();
                if(min_timer <= now) {
                        timeout = 0;
                } else {
                        const int64_t wake = min_timer - now;
                        timeout = timeout < wake ? timeout : wake;
                }
        }
       
        const int event_cnt = epoll_wait(
                worker->epfd, 
                events, 
                server->cfg.event_buffer, 
                (int)timeout);
        SEL_TEST(event_cnt != -1);

        now = law_time_millis();
        while(law_pq_peek(timers, &min_timer, (void**)&task)) {
                if(min_timer > now) break;
                SEL_TEST(law_pq_pop(timers));
                if(!task) continue;
                SEL_TEST(law_tasks_add(ready, task, LAW_SRV_SIGTTL));
        }

        for(int x = 0; x < event_cnt; ++x) {
                struct epoll_event *ep_ev = events + x;
                struct law_event *law_ev = ep_ev->data.ptr;
                if(!law_ev) continue;
                law_ev->revents = ep_ev->events;
                SEL_TEST(law_tasks_add(ready, law_ev->task, LAW_SRV_SIGIO));
        }

        for(int I = 0; I < 0x10000; ++I) {
                const ssize_t nb = read(reader, &task, sizeof(void*));
                if(nb < 0) {
                        SEL_TEST(errno == EWOULDBLOCK || errno == EAGAIN);
                        break;
                }
                SEL_TEST(nb == sizeof(struct law_task*));
                SEL_ASSERT(task);
                SEL_TEST(law_tasks_add(ready, task, LAW_SRV_SIGINT));
        }
        
        return law_worker_dispatch(worker);
}

static sel_err_t law_worker_spin(struct law_worker *worker)
{
        struct law_server *server = worker->server;
        SEL_TRY_QUIETLY(server->cfg.init(worker, server->cfg.data));
        while(  atomic_load(&server->mode) == LAW_SRV_RUNNING || 
                atomic_load(&server->mode) == LAW_SRV_STOPPING || 
                atomic_load(&server->ntasks) > 0 ||
                law_pq_size(worker->timers) > 0) 
        {
                SEL_TRY_QUIETLY(server->cfg.tick(worker, server->cfg.data));
                SEL_TRY_QUIETLY(law_worker_tick(worker));
        }
        return LAW_ERR_OK;
}

static sel_err_t law_worker_run(struct law_worker *w)
{
        SEL_ASSERT(w && w->server && w->channel.fds[0]);
        struct law_server *s = w->server;
        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL };
        const int reader = w->channel.fds[0];
        w->epfd = epoll_create(s->cfg.event_buffer);
        SEL_TEST(w->epfd != -1);
        SEL_TEST(epoll_ctl(w->epfd, EPOLL_CTL_ADD, reader, &ev) != -1);
        const sel_err_t err = law_worker_spin(w);
        epoll_ctl(w->epfd, EPOLL_CTL_DEL, reader, NULL);
        close(w->epfd);
        return err;
}

static void *law_worker_thread(void *state) 
{
        struct law_worker *worker = state;
        law_worker_run(worker);
        return NULL;
}

static sel_err_t law_server_trampoline(
        struct law_worker *worker, 
        struct law_data data)
{
        struct law_server *srv = worker->server;
        return srv->cfg.accept(worker, data.u.fd, srv->cfg.data);
}

static sel_err_t law_server_accept(struct law_server *server) 
{
        FILE *errs = server->cfg.errors;
        while(atomic_load(&server->ntasks) < server->cfg.maxconns) {
                const int fd = accept(server->socket, NULL, NULL);
                if(fd < 0) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) 
                                return LAW_ERR_OK; 
                        law_log_error(errs, "accept", strerror(errno));
                        return SEL_FREPORT(errs, LAW_ERR_SYS);
                }
                const int flags = fcntl(fd, F_GETFL);
                SEL_TEST(flags != -1);
                SEL_TEST(fcntl(fd, F_SETFL, O_NONBLOCK | flags) != -1);
                struct law_data data = { .u.fd = fd };
                const sel_err_t err = law_srv_spawn(
                        server, 
                        law_server_trampoline, 
                        &data);
                if(err != LAW_ERR_OK) {
                        close(fd);
                        law_log_error(
                                errs, 
                                "law_srv_spawn", 
                                sel_strerror(errno));
                        SEL_FREPORT(errs, err);
                        if(err == LAW_ERR_WNTW) sleep(1);
                        else return err;
                } 
        }
        return LAW_ERR_OK;
}

static sel_err_t law_server_spin(struct law_server *s)
{
        while(atomic_load(&s->mode) == LAW_SRV_RUNNING) {
                SEL_TRY_QUIETLY(law_server_accept(s));
                SEL_TEST(epoll_wait(
                        s->epfd, 
                        s->events, 
                        s->cfg.event_buffer, 
                        s->cfg.server_timeout) != -1);
        }
        atomic_store(&s->mode, LAW_SRV_STOPPED);
        return LAW_ERR_OK;
}

static sel_err_t law_server_run_threads(struct law_server *s)
{
        SEL_ASSERT(s);
        struct law_worker **workers = s->workers;
        pthread_t *threads = s->threads;
        const int nthreads = s->cfg.workers;
        SEL_ASSERT(workers && threads && nthreads);
        for(int n = 0; n < nthreads; ++n) {
                struct law_worker *worker = workers[n];
                SEL_ASSERT(worker);
                worker->id = n;
                worker->server = s;
                SEL_TEST(law_pipe_open(&worker->channel) == LAW_ERR_OK);
                SEL_TEST(pthread_create(
                        threads + n,
                        NULL, 
                        law_worker_thread, 
                        worker) == 0);
        }
        const sel_err_t err = law_server_spin(s);
        for(int m = 0; m < nthreads; ++m) {
                pthread_join(threads[m], NULL);
                law_pipe_close(&workers[m]->channel);
        }
        return err;
}

static sel_err_t law_server_run_epoll(struct law_server *s)
{
        struct epoll_event ev;
        sel_err_t err;
        s->epfd = epoll_create(2);
        SEL_TEST(s->epfd != -1);
        ev.data.ptr = &s->socket;
        ev.events = EPOLLIN | EPOLLERR;
        SEL_TEST(epoll_ctl(s->epfd, EPOLL_CTL_ADD, s->socket, &ev) != -1);
        err = law_server_run_threads(s);
        epoll_ctl(s->epfd, EPOLL_CTL_DEL, s->socket, NULL);
        close(s->epfd);
        return err;
}

static sel_err_t law_server_run(struct law_server *s)
{
        return law_server_run_epoll(s);
}

sel_err_t law_srv_start(struct law_server *s)
{
        if(atomic_load(&s->mode) != LAW_SRV_CREATED) {
                return LAW_ERR_MODE;
        }
        atomic_store(&s->mode, LAW_SRV_RUNNING);
        return law_server_run(s);
}

sel_err_t law_srv_stop(struct law_server *s)
{
        atomic_store(&s->mode, LAW_SRV_STOPPING);
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
                        return SEL_HALT();
        }

        const int fd = socket(domain, type, 0);
        if(fd < 0) { 
                law_log_error(errs, "socket", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        }
        const int flags = fcntl(fd, F_GETFL);
        SEL_TEST(flags != -1);
        SEL_TEST(fcntl(fd, F_SETFL, O_NONBLOCK | flags) != -1);
        if(bind(fd, (struct sockaddr*)&addr, addrlen) < 0) {
                law_log_error(errs, "bind", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        } else if(listen(fd, s->cfg.backlog) < 0) { 
                law_log_error(errs, "listen", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        } else if(setgid(s->cfg.gid) < 0) { 
                law_log_error(errs, "setgid", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        } else if(setuid(s->cfg.uid) < 0) {
                law_log_error(errs, "setuid", strerror(errno));
                return SEL_FREPORT(errs, SEL_ERR_SYS);
        }

        s->socket = fd;

        return SEL_ERR_OK;
}

sel_err_t law_srv_close(struct law_server *server)
{
        SEL_IO_QUIETLY(close(server->socket));
        return SEL_ERR_OK;
}

sel_err_t law_srv_await1(
        struct law_worker *worker,
        const int fd,
        const int64_t timeout,
        law_srv_io1_t callback,
        void *arg)
{
        struct law_event ev; 
        memset(&ev, 0, sizeof(struct law_event));
        int64_t now = law_time_millis();
        const int64_t wakeup = timeout + now;
        for(int I = 0; I < 512; ++I) {
                sel_err_t err = callback(arg);
                int event;
                switch(err) {
                        case LAW_ERR_OK: 
                                return LAW_ERR_OK;
                        case LAW_ERR_WNTW: 
                                event = LAW_SRV_POLLOUT;
                                break;
                        case LAW_ERR_WNTR: 
                                event = LAW_SRV_POLLIN;
                                break;
                        default: 
                                return err;
                }
                now = law_time_millis();
                if(wakeup <= now) {
                        return LAW_ERR_TTL;
                } else if(event != ev.events) {
                        SEL_TRY_QUIETLY(law_srv_mod(worker, fd, &ev));
                } if((err = law_srv_sleep(worker, wakeup - now)) < 0) {
                        return err; 
                }               
        }
        return LAW_ERR_TTL;
}

struct law_srv_await2_state {
        law_srv_io2_t callback;
        void *arg0, *arg1;
};

sel_err_t law_srv_await2_run(void *arg)
{
        struct law_srv_await2_state *state = arg;
        return state->callback(state->arg0, state->arg1);
}

sel_err_t law_srv_await2(
        struct law_worker *worker,
        const int fd,
        const int64_t timeout,
        law_srv_io2_t callback,
        void *arg0,
        void *arg1)
{
        struct law_srv_await2_state state = { callback, arg0, arg1 };
        return law_srv_await1(
                worker,
                fd, 
                timeout,
                law_srv_await2_run,
                &state);
}