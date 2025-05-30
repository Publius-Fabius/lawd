/** server.h implementation */

/* See man getaddrinfo */
#define _POSIX_C_SOURCE 200112L

#include "lawd/error.h"
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/time.h"
#include "lawd/priority.h"
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

static const char *law_srv_dets(sel_err_t err) 
{
        return err == LAW_ERR_SYS ? strerror(errno) : "";
}

#define LAW_SRV_ERR(F, A, E) LAW_LOG_ERR(F, A, E, law_srv_dets(E))

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
        cfg.server_events       = 32;
        cfg.worker_events       = 32;
        cfg.workers             = 1;
        cfg.data                = (struct law_data){ .u64 = 0 };
        cfg.init                = NULL;
        cfg.tick                = NULL;
        cfg.accept              = NULL;
        cfg.errors              = stderr;
        return cfg;
}

enum law_srv_mode {
        LAW_SRV_CREATED         = 1,            /** Server Created */
        LAW_SRV_RUNNING         = 2,            /** Server Running */
        LAW_SRV_STOPPING        = 3,            /** Server Stopping */
        LAW_SRV_STOPPED         = 4,            /** Server Stopped */
        LAW_SRV_SPAWNED         = 5,            /** Task Spawned */
        LAW_SRV_SUSPENDED       = 6,            /** Task Suspended */
        LAW_SRV_ENQUEUED        = 7,            /** Task Enqueued */
        LAW_SRV_PROCESSING      = 8             /** Task Processing */
};

struct law_task {
        int mode;                               /** Running Mode */
        int mark;                               /** Ready Set Bit */
        int resched;                            /** Reschedule Bit */
        uint32_t version;                       /** Version Number */
        int max_events;                         /** Maximum Events */
        int num_events;                         /** Number of Events */
        struct law_ev *events;                  /** User's Event Array */
        pthread_mutex_t lock;                   /** Notification Lock */
        struct law_worker *worker;              /** Task's Worker */
        struct law_cor *callee;                 /** Coroutine Callee */
        struct law_smem *stack;                 /** Coroutine Stack */
        law_srv_call_t callback;                /** User Callback */
        struct law_data data;                   /** User Data */
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
        struct law_pipe channel;
        struct law_server *server;
        struct law_cor *caller;
        struct law_pq *timers;
        struct law_tasks *ready;
        struct law_task *active;
        struct law_evo *evo;
};

struct law_server {
        struct law_srv_cfg cfg;
        volatile int mode;
        volatile int ntasks;
        int socket;
        int spawn_counter;
        pthread_t *threads;
        struct law_worker **workers;
        struct law_evo *evo;
};

struct law_tasks *law_tasks_create()
{
        struct law_tasks *tasks = calloc(1, sizeof(struct law_tasks));
        if(!tasks) goto FAILURE;
        tasks->array = calloc(LAW_INIT_TASKS_CAP, sizeof(void*));
        if(!tasks->array) goto FREE_TASKS;
        tasks->capacity = LAW_INIT_TASKS_CAP;
        return tasks;
        FREE_TASKS:
        free(tasks);
        FAILURE:
        return NULL;
}

void law_tasks_destroy(struct law_tasks *tasks)
{
        if(!tasks) return;
        free(tasks->array);
        free(tasks);
}

struct law_tasks *law_tasks_reset(struct law_tasks *tasks)
{
        SEL_ASSERT(tasks);
        tasks->size = 0;
        return tasks;
}

struct law_tasks *law_tasks_grow(struct law_tasks *ps)
{
        const size_t old_cap = ps->capacity;
        const size_t new_cap = old_cap * 2;
        struct law_task **addr = realloc(ps->array, new_cap * sizeof(void*));
        if(!addr) return NULL;
        ps->array = addr;
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
        SEL_ASSERT(tasks->size <= tasks->capacity);
        if(task->mark) {
                return tasks;
        } else if(tasks->capacity == tasks->size) {
                if(!law_tasks_grow(tasks)) 
                        return NULL;
        }
        tasks->array[tasks->size++] = task;
        task->mark = 1;
        return tasks;
}

static void law_srv_closepipe(struct law_pipe *p)
{
        close(p->fds[0]);
        close(p->fds[1]);
}

static sel_err_t law_srv_openpipe(struct law_pipe *p)
{
        if(pipe(p->fds) < 0) return LAW_ERR_SYS;
        int flags = fcntl(p->fds[0], F_GETFL);
        SEL_TEST(flags != -1);
        SEL_TEST(fcntl(p->fds[0], F_SETFL, O_NONBLOCK | flags) != -1);
        SEL_TEST((flags = fcntl(p->fds[1], F_GETFL)) != -1);
        SEL_TEST(fcntl(p->fds[1], F_SETFL, O_NONBLOCK | flags) != -1);
        return LAW_ERR_OK;
}

static sel_err_t law_worker_enq(
        struct law_worker *worker, 
        struct law_task *task)
{
        const int writer = worker->channel.fds[1];
        const ssize_t written = write(writer, &task, sizeof(void*));
        if(written < 0) {
                if(errno == EWOULDBLOCK || errno == EAGAIN)
                        return LAW_ERR_WNTW;
                else SEL_HALT();
        }
        SEL_TEST(written == sizeof(void*));
        return LAW_ERR_OK;
}

static int law_srv_lessthan(struct law_data a, struct law_data b) 
{
        return a.i64 < b.i64;
}

struct law_worker *law_worker_create(
        struct law_server *server,
        const int id)
{
        SEL_ASSERT(server);

        struct law_worker *w = calloc(1, sizeof(struct law_worker));
        if(!w) goto FAILURE;
        w->caller = law_cor_create();
        if(!w->caller) goto FREE_WORKER;
        w->ready = law_tasks_create();
        if(!w->ready) goto FREE_CALLER;
        w->timers = law_pq_create(law_srv_lessthan);
        if(!w->timers) goto FREE_READY;
        w->evo = law_evo_create(server->cfg.worker_events);
        if(!w->evo) goto FREE_TIMERS;
        w->server = server;
        w->id = id;
        return w;
        FREE_TIMERS:
        law_pq_destroy(w->timers);
        FREE_READY:
        law_tasks_destroy(w->ready);
        FREE_CALLER:
        law_cor_destroy(w->caller);
        FREE_WORKER:
        free(w);
        FAILURE:
        return NULL;
}

void law_worker_destroy(struct law_worker *w)
{
        if(!w) return;
        law_evo_destroy(w->evo);
        law_pq_destroy(w->timers);
        law_tasks_destroy(w->ready);
        law_cor_destroy(w->caller);
        free(w);
}

struct law_server *law_srv_create(struct law_srv_cfg *cfg) 
{
        SEL_ASSERT(cfg);
        const int nthreads = cfg->workers;
        const int nevents = cfg->server_events;
        SEL_ASSERT(0 < nthreads && nthreads < 0x1000);
        SEL_ASSERT(0 < nevents && nevents < 0x40000);
        int n = 0;
        struct law_server *srv = calloc(1, sizeof(struct law_server));
        if(!srv) goto FAILURE;
        srv->cfg = *cfg;
        srv->mode = LAW_SRV_CREATED;
        srv->threads = calloc((size_t)nthreads, sizeof(pthread_t));
        if(!srv->threads) goto FREE_SERVER;
        srv->workers = calloc((size_t)nthreads, sizeof(struct law_worker*));
        if(!srv->workers) goto FREE_THREADS;
        srv->evo = law_evo_create(nevents);
        if(!srv->evo) goto FREE_WORKERS;
        for(; n < nthreads; ++n) {
                srv->workers[n] = law_worker_create(srv, n);
                if(!srv->workers[n]) goto FREE_ALL;
        }
        return srv;
        FREE_ALL:
        for(int m = 0; m < n; ++m) {
                law_worker_destroy(srv->workers[m]);
        }
        law_evo_destroy(srv->evo);
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
        if(!srv) return;
        for(int n = 0; n < srv->cfg.workers; ++n) {
                law_worker_destroy(srv->workers[n]);
        }
        law_evo_destroy(srv->evo);
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
        struct law_data data,
        const size_t stack_length,
        const size_t stack_guard)
{
        struct law_task *task = calloc(1, sizeof(struct law_task));
        if(!task) goto FAILURE;
        task->mode = LAW_SRV_SPAWNED;
        task->version = 0;
        task->events = NULL;
        task->max_events = task->num_events = 0;
        task->callback = callback;
        task->data = data;
        task->callee = law_cor_create();
        if(!task->callee) goto FREE_TASK;
        task->stack = law_smem_create(stack_length, stack_guard);
        if(!task->stack) goto FREE_COR;
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
        if(!task) return;
        law_smem_destroy(task->stack);
        law_cor_destroy(task->callee);
        free(task);
}

struct law_server *law_srv_server(struct law_worker *worker) 
{
        return worker->server;
}

sel_err_t law_srv_ectl(
        struct law_worker *w,
        struct law_slot *slot,
        const int op,
        const uint32_t flags,
        const uint32_t events)
{
        SEL_ASSERT(w);
        struct law_ev ev = { .events = events, .data = { .ptr = slot } };
        return law_evo_ctl(w->evo, slot->fd, op, flags, &ev);
}

int law_srv_ewait(
        struct law_worker *w,
        const int64_t expiry,
        struct law_ev *events,
        const int max_events)
{
        SEL_ASSERT(w && w->active);
        struct law_task *task = w->active;
        
        if(expiry <= law_time_millis()) return LAW_ERR_TTL;
        struct law_data pri = { .i64 = expiry };
        if(!law_pq_insert(w->timers, pri, task->version, task))
                return LAW_ERR_OOM;
        task->max_events = max_events;
        task->num_events = 0;
        task->events = events;
        law_cor_yield(w->caller, task->callee, LAW_SRV_SUSPENDED);
        return task->num_events;
}

sel_err_t law_srv_notify(struct law_task *task)
{
        SEL_ASSERT(task);
        struct law_worker *worker = task->worker;
        sel_err_t err = -1;
        (void)pthread_mutex_lock(&task->lock);
        switch(task->mode) {
                case LAW_SRV_SUSPENDED:
                        err = law_worker_enq(worker, task);
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
        struct law_server *srv,
        law_srv_call_t fun,
        struct law_data data)
{
        SEL_ASSERT(srv);
        struct law_task *task = law_task_create(
                fun,
                data,
                srv->cfg.stack_length,
                srv->cfg.stack_guard);
        if(!task) return LAW_ERR_OOM;
        const int index = srv->spawn_counter;
        srv->spawn_counter = (index + 1) % srv->cfg.workers;
        const sel_err_t err = law_worker_enq(srv->workers[index], task);
        if(err != LAW_ERR_OK) {
                law_task_destroy(task);
                return err;
        }
        atomic_fetch_add(&srv->ntasks, 1);
        return LAW_ERR_OK;
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
                ++task->version;
                (void)pthread_mutex_lock(&task->lock);
                const int mode = task->mode;
                task->mode = LAW_SRV_PROCESSING;
                (void)pthread_mutex_unlock(&task->lock);
                const sel_err_t sig = mode == LAW_SRV_SPAWNED ?
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
                        else if(law_worker_enq(w, task) == LAW_ERR_OK)
                                task->mode = LAW_SRV_ENQUEUED;
                        else task->mode = LAW_SRV_SUSPENDED;
                        task->resched = 0;
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

static void law_task_pushev(struct law_task *task, struct law_ev *ev)
{
        SEL_ASSERT(task->events && task->num_events <= task->max_events);
        if(task->num_events < task->max_events)
                task->events[task->num_events++] = *ev;
}

static sel_err_t law_worker_tick(struct law_worker *worker)
{
        struct law_server *server = worker->server;
        struct law_pq *timers = worker->timers;
        struct law_tasks *ready = worker->ready;
        const int reader = worker->channel.fds[0];

        int64_t timeout = server->cfg.worker_timeout;
        struct law_data min_timer;
        int64_t now = 0;
       
        if(law_pq_peek(timers, &min_timer, NULL, NULL)) {
                now = law_time_millis();
                if(min_timer.i64 <= now) {
                        timeout = 0;
                } else {
                        const int64_t wake = min_timer.i64 - now;
                        timeout = timeout < wake ? timeout : wake;
                }
        }
       
        SEL_TEST(law_evo_wait(worker->evo, (int)timeout) >= 0);

        uint32_t version;
        struct law_task *task = NULL;
        struct law_ev ev = { .events = LAW_EV_TTL };
        now = law_time_millis();

        while(law_pq_peek(timers, &min_timer, &version, (void**)&task)) {
                SEL_ASSERT(task);
                if(min_timer.i64 > now) break;
                SEL_TEST(law_pq_pop(timers, NULL, NULL, NULL));
                if(task->version != version) continue;
                law_task_pushev(task, &ev);
                SEL_TEST(law_tasks_add(ready, task));
        }

        while(law_evo_next(worker->evo, &ev)) {
                struct law_slot *slot = ev.data.ptr;
                if(!slot) continue;
                SEL_ASSERT(slot->task);
                law_task_pushev(slot->task, &ev);
                SEL_TEST(law_tasks_add(ready, slot->task));
        }

        ev.events = LAW_EV_INT;
        ev.data.ptr = NULL;

        for(int I = 0; I < 0x8000; ++I) {
                struct law_task *task = NULL;
                const ssize_t nb = read(reader, &task, sizeof(void*));
                if(nb < 0) {
                        SEL_TEST(errno == EWOULDBLOCK || errno == EAGAIN);
                        break;
                }
                SEL_ASSERT(task);
                SEL_TEST(nb == sizeof(struct law_task*));
                if(task->mode == LAW_SRV_ENQUEUED) 
                        law_task_pushev(task, &ev);
                SEL_TEST(law_tasks_add(ready, task));
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
        const int in = w->channel.fds[0];
        struct law_ev ev = { .events = LAW_EV_R, .data.ptr = NULL };
        SEL_TEST(law_evo_ctl(w->evo, in, LAW_EV_ADD, 0, &ev) != -1);
        const sel_err_t err = law_worker_spin(w);
        SEL_TEST(law_evo_ctl(w->evo, in, LAW_EV_DEL, 0, &ev) != -1);
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
        return srv->cfg.accept(worker, data.fd, srv->cfg.data);
}

static sel_err_t law_server_accept_spawn(
        struct law_server *server,
        const int fd) 
{
        FILE *errs = server->cfg.errors;
        struct law_data data = { .fd = fd };
        sel_err_t err = LAW_ERR_WNTW;
        for(int I = 0; I < 8; ++I) {
                err = law_srv_spawn(
                        server, 
                        law_server_trampoline, 
                        data);
                if(err == LAW_ERR_WNTW) {
                        struct timespec ts;
                        memset(&ts, 0, sizeof(struct timespec));
                        ts.tv_nsec = 1000000 * 50 * (1 << I);
                        SEL_TEST(nanosleep(&ts, NULL) != -1);
                } else if(err == LAW_ERR_OK) {
                        return LAW_ERR_OK;
                } else {
                        break;
                }
        }
        return LAW_SRV_ERR(errs, "law_srv_spawn", err);
}

static sel_err_t law_server_accept(struct law_server *server) 
{
        FILE *errs = server->cfg.errors;
        while(atomic_load(&server->ntasks) < server->cfg.maxconns) {
                const int fd = accept(server->socket, NULL, NULL);
                if(fd < 0) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) 
                                return LAW_ERR_OK; 
                        return LAW_SRV_ERR(errs, "accept", LAW_ERR_SYS);
                }
                const int flags = fcntl(fd, F_GETFL);
                SEL_TEST(flags != -1);
                SEL_TEST(fcntl(fd, F_SETFL, O_NONBLOCK | flags) != -1);
                if(law_server_accept_spawn(server, fd) != LAW_ERR_OK) {
                        close(fd);
                        break;
                } 
        }
        return LAW_ERR_OK;
}

static sel_err_t law_server_spin(struct law_server *s)
{
        while(atomic_load(&s->mode) == LAW_SRV_RUNNING) {
                SEL_TRY_QUIETLY(law_server_accept(s));
                SEL_TEST(law_evo_wait(s->evo, s->cfg.server_timeout) >= 0);
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
                SEL_TEST(law_srv_openpipe(&worker->channel) == LAW_ERR_OK);
                SEL_TEST(pthread_create(
                        threads + n,
                        NULL, 
                        law_worker_thread, 
                        worker) == 0);
        }
        const sel_err_t err = law_server_spin(s);
        for(int m = 0; m < nthreads; ++m) {
                pthread_join(threads[m], NULL);
                law_srv_closepipe(&workers[m]->channel);
        }
        return err;
}

static sel_err_t law_server_run_epoll(struct law_server *s)
{
        struct law_ev ev = { 
                .events = LAW_EV_R | LAW_EV_ERR, 
                .data = { .ptr = NULL } };
        SEL_TEST(law_evo_ctl(s->evo, s->socket, LAW_EV_ADD, 0, &ev) != -1);
        sel_err_t err = law_server_run_threads(s);
        SEL_TEST(law_evo_ctl(s->evo, s->socket, LAW_EV_DEL, 0, &ev) != -1);
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
                        SEL_HALT();
        }

        const int fd = socket(domain, type, 0);
        if(fd < 0) return LAW_SRV_ERR(errs, "socket", SEL_ERR_SYS);
        const int flags = fcntl(fd, F_GETFL);
        SEL_TEST(flags != -1);
        SEL_TEST(fcntl(fd, F_SETFL, O_NONBLOCK | flags) != -1);
        if(bind(fd, (struct sockaddr*)&addr, addrlen) < 0) {
                return LAW_SRV_ERR(errs, "bind", SEL_ERR_SYS);
        } else if(listen(fd, s->cfg.backlog) < 0) { 
                return LAW_SRV_ERR(errs, "listen", SEL_ERR_SYS);
        } else if(setgid(s->cfg.gid) < 0) { 
                return LAW_SRV_ERR(errs, "setgid", SEL_ERR_SYS);
        } else if(setuid(s->cfg.uid) < 0) {
                return LAW_SRV_ERR(errs, "setuid", SEL_ERR_SYS);
        }

        s->socket = fd;

        return SEL_ERR_OK;
}

sel_err_t law_srv_close(struct law_server *server)
{
        SEL_IO_QUIETLY(close(server->socket));
        return SEL_ERR_OK;
}
