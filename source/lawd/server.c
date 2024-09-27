#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/time.h"
#include "lawd/pqueue.h"
#include "lawd/cqueue.h"

#include <unistd.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>

#define LAW_EVENT_BUF_LEN 1024

struct law_task {
        struct law_cor *callee;
        struct law_smem *stack;
        law_srv_call_t callback;
        void *state;
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
        struct epoll_event *events[LAW_EVENT_BUF_LEN];
        int epfd;
        int id;
};

struct law_server {
        struct law_cqueue *channel;
        struct law_srv_cfg *cfg;
        int mode;
        int alertfd;
        int socket;
};

enum law_srv_ctrl {
        LAW_SRV_SPAWNED         = 1,
        LAW_SRV_YIELDED         = 2,
        LAW_SRV_RUNNING         = 3
};

static struct law_tasks *law_tasks_reset(struct law_tasks *tasks);

static struct law_tasks *law_tasks_add(
        struct law_tasks *tasks, 
        struct law_task *task);

sel_err_t law_srv_watch(struct law_worker *worker, const int sock)
{
        SEL_IO_QUIETLY(epoll_ctl(worker->epfd, EPOLL_CTL_ADD, sock, NULL));
        return LAW_ERR_OK;
}

sel_err_t law_srv_unwatch(struct law_worker *worker,const int sock)
{
        SEL_IO_QUIETLY(epoll_ctl(worker->epfd, EPOLL_CTL_DEL, sock, NULL));
        return LAW_ERR_OK;
}

static int law_srv_to_epoll_ev(int law_ev) 
{
        int ev = 0;
        if(law_ev & LAW_SRV_PIN) {
                ev |= EPOLLIN;
        } 
        if(law_ev & LAW_SRV_POUT) {
                ev |= EPOLLOUT;
        }
        if(law_ev & LAW_SRV_PHUP) {
                ev |= EPOLLHUP;
        }
        if(law_ev & LAW_SRV_PERR) {
                ev |= EPOLLERR;
        }
        return ev;
}

static int law_srv_from_epoll_ev(int ep_ev) 
{
        int l_ev = 0;
        if(ep_ev & EPOLLIN) {
                l_ev |= LAW_SRV_PIN;
        } 
        if(ep_ev & EPOLLOUT) {
                l_ev |= LAW_SRV_POUT;
        }
        if(ep_ev & EPOLLHUP) {
                l_ev |= LAW_SRV_PHUP;
        }
        if(ep_ev & EPOLLERR) {
                l_ev |= LAW_SRV_PERR;
        }
        return l_ev;
}

sel_err_t law_srv_poll(struct law_worker *worker, struct law_event *lev)
{
        struct epoll_event eev;
        eev.data.ptr = lev;
        eev.events = law_srv_to_epoll_ev(lev->flags) | EPOLLONESHOT;
        SEL_IO_QUIETLY(epoll_ctl(
                worker->epfd, 
                EPOLL_CTL_ADD, 
                lev->socket, 
                &eev));
        return LAW_ERR_OK;
}

sel_err_t law_srv_wait(struct law_worker *worker, int64_t timeout)
{
        const int64_t current_time = law_time_millis();
        const int64_t wakeup = current_time + timeout;
        if(!law_pq_insert(worker->timers, worker->active, wakeup)) {
                LAW_ERR_OOM;
        }
        return law_cor_yield(
                worker->caller, 
                worker->active->callee, 
                LAW_SRV_YIELDED);
}

struct law_task *law_task_create(
        law_srv_call_t callback,
        void *state,
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
        task->state = state;
        task->stack = law_smem_create(stack_length, stack_guard);

        if(!task->stack) {
                free(task);
                return NULL;
        }

        return task;
}

void law_task_destroy(struct law_task *task) 
{
        law_smem_destroy(task->stack);
        free(task);
}

sel_err_t law_srv_spawn(
        struct law_server *server,
        law_srv_call_t callback,
        void *state)
{
        struct law_task *task = law_task_create(
                callback,
                state,
                server->cfg->stack_length,
                server->cfg->stack_guard);
        if(!task) {
                return LAW_ERR_OOM;
        }
        if(!law_cq_enq(server->channel, task)) {
                law_task_destroy(task);
                return LAW_ERR_OOM;
        }
        return LAW_ERR_OK;
}

static int law_srv_trampoline(
        struct law_cor *init_env, 
        struct law_cor *cor_env, 
        void *state)
{
        struct law_worker *worker = state;
        struct law_task *task = worker->active;
        return task->callback(worker, task->state);
}

static sel_err_t law_srv_dispatch(struct law_worker *worker)
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
                sel_err_t sig;

                if(mode == LAW_SRV_SPAWNED) {
                        worker->active = task;
                        sig = law_cor_call(
                                worker->caller, 
                                task->callee, 
                                law_smem_address(task->stack),
                                law_srv_trampoline,
                                worker);
                } else if(mode == LAW_SRV_YIELDED) {
                        worker->active = task;
                        sig = law_cor_resume(
                                worker->caller,
                                task->callee,
                                LAW_ERR_OK);
                } else {
                        return SEL_ABORT();
                }

                if(sig == LAW_SRV_YIELDED) {
                        task->mode = LAW_SRV_YIELDED;
                        task->mark = 0;
                } if(sig == LAW_ERR_OK) {
                        law_task_destroy(task);
                } else {
                        law_task_destroy(task);
                        return sig;
                }
        }
        law_tasks_reset(ready);
        return LAW_ERR_OK;
}

static sel_err_t law_srv_pump(struct law_worker *worker)
{
        int64_t timeout = worker->server->cfg->timeout;
        int64_t min_time;
        if(law_pq_peek(worker->timers, NULL, &min_time)) {
                const int64_t current_time = law_time_millis();
                if(min_time < current_time) {
                        timeout = 0;
                } else {
                        const int64_t diff = min_time - current_time;
                        timeout = timeout < diff ? timeout : diff;
                }
        }

        struct epoll_event *events = worker->events;
        const int event_cnt = epoll_wait(
                worker->epfd, 
                events, 
                LAW_EVENT_BUF_LEN, 
                timeout);
        if(event_cnt < 0) {
                return SEL_REPORT(LAW_ERR_SYS);
        }

        struct law_tasks *ready = worker->ready;

        for(int x = 0; x < event_cnt; ++ x) {
                struct law_event *lev = events[x].data.ptr;
                lev->flags = law_srv_from_epoll_ev(events[x].events);
                law_tasks_add(ready, lev->task);
        }

        struct law_pqueue *timers = worker->timers;
        struct law_task *task;
        int64_t timer;
        const int64_t current_time = law_time_millis();
        
        while(law_pq_peek(timers, &task, &timer)) {
                if(timer < current_time) {
                        law_tasks_add(ready, task);
                        law_pq_pop(timers);
                } else {
                        break;
                }
        }

        return law_srv_dispatch(worker);
}

sel_err_t law_srv_spin_worker(struct law_worker *worker)
{
        struct law_server *server = worker->server;
        struct law_timers *timers = worker->timers;

        while(  server->mode == LAW_SRV_RUNNING 
                || law_pq_size(timers)
                || law_cq_size(server->channel)) {

                struct law_task *task;
                if(law_cq_deq(server->channel, &task)) {
                        law_tasks_add(worker->ready, task);
                }

                SEL_TRY_QUIETLY(law_srv_pump(worker));
        }
        return LAW_ERR_OK;
}

sel_err_t law_err_run_worker(struct law_worker *worker)
{
        struct law_server *server = worker->server;
        struct law_timers *timers = worker->timers;

        worker->epfd = epoll_create(2);
       
        struct epoll_event ev; 
        ev.events = EPOLLIN;

        if(epoll_ctl(worker->epfd, EPOLL_CTL_ADD, server->alertfd, &ev) < 0) {
                epoll_destroy(worker->epfd);
                perror("epoll_ctl");
                return SEL_REPORT(LAW_ERR_SYS);
        }

        sel_err_t err = law_srv_spin_worker(worker);

        epoll_ctl(worker->epfd, EPOLL_CTL_DEL, server->alertfd, NULL);
        epoll_destroy(worker->epfd);

        return err;
}

static sel_err_t law_srv_accept(struct law_worker *worker, void *state)
{
        struct law_server *server = worker->server;
        int socket = (int)state;
        return server->cfg->accept(worker, socket, server->cfg->state);
}

static sel_err_t law_srv_spin_server(struct law_server *server)
{

        int client = accept(server->socket, NULL, NULL);

        law_srv_spawn(server, law_srv_accept, (void*)client);
}

static void *law_srv_thread(void *state) 
{
        struct law_worker *worker = state;
        law_err_run_worker(worker);
}

sel_err_t law_srv_run(struct law_server *server)
{
        int pipefd[2];
        if(pipe(pipefd) == -1) {
                SEL_REPORT(SEL_ERR_SYS);
                perror("pipe");
                exit(EXIT_FAILURE);
        }
        server->alertfd = pipefd[1];

        const size_t nworkers = server->cfg->workers;
        int *threads = malloc(sizeof(int) * nworkers);
        struct law_worker *workers = malloc(
                sizeof(struct law_worker) * nworkers);
        
        struct law_worker *worker;
        for(size_t n = 0; n < nworkers; ++n) {
                workers[n].id = n;
                pthread_create(threads + n, NULL, law_srv_thread, workers + n);
        }
       
        law_srv_spin_server(server);

        for(size_t n = 0; n < nworkers; ++n) {
                pthread_join(threads + n, NULL);
        }

        free(workers);
        free(threads);
        close(pipefd[0]);
        close(pipefd[1]);

        return LAW_ERR_OK;
}