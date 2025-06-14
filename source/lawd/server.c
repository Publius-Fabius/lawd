/** server.h implementation */

/* See man getaddrinfo */
#define _POSIX_C_SOURCE 200112L

#include "lawd/error.h"
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/table.h"
#include "lawd/timer.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdatomic.h>

law_server_config_t law_server_sanity()
{
        law_server_config_t cfg;

        cfg.protocol            = LAW_PROTOCOL_TCP;
        cfg.port                = 80;
        cfg.backlog             = 10;

        cfg.uid                 = 1000;
        cfg.gid                 = 1000;

        cfg.workers             = 1;  
        cfg.worker_tasks        = 8;

        cfg.worker_timeout      = 5000;
        cfg.server_timeout      = 10000;

        cfg.stack               = 0x100000;
        cfg.guards               = 0x1000;
        
        cfg.server_events       = 32;
        cfg.worker_events       = 32;
        
        cfg.init                = NULL;
        cfg.tick                = NULL;
        cfg.accept              = NULL;
        cfg.data                = (struct law_data){ .u64 = 0 };
        
        return cfg;
}

enum law_server_mode {                          /** Server Mode */
        LAW_MODE_CREATED         = 1,           /** Server Created */
        LAW_MODE_RUNNING         = 2,           /** Server Running */
        LAW_MODE_STOPPING        = 3,           /** Server Stopping */
        LAW_MODE_STOPPED         = 4,           /** Server Stopped */
        LAW_MODE_SPAWNED         = 5,           /** Task Spawned */
        LAW_MODE_SUSPENDED       = 6,           /** Task Suspended */
};

typedef struct law_task {
        int mode;                               /** Running Mode */
        int mark;                               /** Ready Set Bit */
        int max_events;                         /** Maximum Events */
        int num_events;                         /** Number of Events */
        law_version_t version;                  /** Version Number */
        law_id_t id;                            /** Task Identifier */
        law_event_t *events;                    /** User's Event Array */
        law_task_t *next;                       /** Next Task */
        law_worker_t *worker;                   /** Task's Worker */
        law_cor_t *callee;                      /** Coroutine Callee */
        law_smem_t *stack;                      /** Coroutine Stack */
        law_callback_t callback;                /** User Callback */
        law_data_t data;                        /** User Data */
} law_task_t;

enum law_msg_type {                             /** Message Type */
        LAW_MSG_SHUTDOWN        = 1,            /** Prepare for Shutdown */
        LAW_MSG_WAKEUP          = 2             /** Wakeup Notification */
};

/** Message */
typedef struct law_msg {
        int type;
        law_data_t data;
} law_msg_t;

/** Message Queue */
typedef struct law_msg_queue {
        int fds[2];
} law_msg_queue_t;

/** Task Queue */
typedef struct law_task_queue {
        int fds[2];
} law_task_queue_t;

/** Task Ready Set */
typedef struct law_ready_set {
        law_task_t *list;
} law_ready_set_t;

struct law_worker {
        int mode;
        law_msg_queue_t messages;
        law_task_queue_t incoming;
        law_ready_set_t ready;
        law_server_t *server;
        law_cor_t *caller;
        law_table_t *table;
        law_timer_t *timer;
        law_task_t *active;
        law_evo_t *evo;
};

/** Task Pool */
typedef struct law_task_pool {
        pthread_mutex_t lock;
        law_task_t *list;
        size_t capacity, size;
} law_task_pool_t;

struct law_server {
        law_server_config_t cfg;
        int socket;
        int mode;
        atomic_uint_fast64_t seed;
        law_task_pool_t *pool;
        pthread_t *threads;
        law_worker_t **workers;
        law_evo_t *evo;
};

/** Open a non-blocking pipe. */
static int law_pipe_open(int *fds)
{
        if(pipe(fds) == -1) return LAW_ERR_SYS;

        int flags;

        if((flags = fcntl(fds[0], F_GETFL)) == -1) 
                return LAW_ERR_SYS;
        if(fcntl(fds[0], F_SETFL, O_NONBLOCK | flags) == -1)
                return LAW_ERR_SYS;

        if((flags = fcntl(fds[1], F_GETFL)) == -1)
                return LAW_ERR_SYS;
        if(fcntl(fds[1], F_SETFL, O_NONBLOCK | flags) == -1)
                return LAW_ERR_SYS;
        
        return LAW_ERR_OK;
}

/** Close a pipe. */
static void law_pipe_close(int *fds)
{
        close(fds[0]);
        close(fds[1]);
}

/** Write nbytes of addr to the pipe. */
static sel_err_t law_pipe_write(int *fds, void *addr, size_t nbytes)
{
        const ssize_t result = write(fds[1], addr, nbytes);

        if(result == -1) {
                if(errno == EWOULDBLOCK || errno == EAGAIN) 
                        return LAW_ERR_WNTW;
                else 
                        return LAW_ERR_SYS;
        } 

        SEL_TEST(result == nbytes);

        return LAW_ERR_OK;
}

/** Read exactly nbytes to address. */
static sel_err_t law_pipe_read(int *fds, void *addr, size_t nbytes)
{
        const ssize_t result = read(fds[0], addr, nbytes);

        if(result == -1) {
                if(errno == EWOULDBLOCK || errno == EAGAIN)
                        return LAW_ERR_WNTR;
                else 
                        return LAW_ERR_SYS;
        } 

        SEL_TEST(result == nbytes);

        return LAW_ERR_OK;
}

/** Return LAW_ERR_SYS or LAW_ERR_OK.  */
sel_err_t law_msg_queue_open(law_msg_queue_t *queue)
{
        SEL_ASSERT(queue);
        return law_pipe_open(queue->fds);
}

/** Close the message queue. */
void law_msg_queue_close(law_msg_queue_t *queue)
{
        SEL_ASSERT(queue);
        law_pipe_close(queue->fds);
}

/** Returns LAW_ERR_SYS, LAW_ERR_WNTW, LAW_ERR_OK. */
sel_err_t law_msg_queue_push(law_msg_queue_t *queue, law_msg_t *msg)
{
        return law_pipe_write(queue->fds, msg, sizeof(law_msg_t));
}

/** Returns LAW_ERR_SYS, LAW_ERR_WNTR, LAW_ERR_OK.*/
sel_err_t law_msg_queue_pop(law_msg_queue_t *queue, law_msg_t *msg)
{
        return law_pipe_read(queue->fds, msg, sizeof(law_msg_t));
}

/** Returns LAW_ERR_SYS or LAW_ERR_OK.  */
sel_err_t law_task_queue_open(law_task_queue_t *queue)
{
        SEL_ASSERT(queue);
        return law_pipe_open(queue->fds);
}

/** Close the channel. */
void law_task_queue_close(law_task_queue_t *queue)
{
        SEL_ASSERT(queue);
        law_pipe_close(queue->fds);
}

/** Returns LAW_ERR_SYS, LAW_ERR_WNTW, LAW_ERR_OK. */
sel_err_t law_task_queue_push(law_task_queue_t *queue, law_task_t *task)
{
        return law_pipe_write(queue->fds, &task, sizeof(void*));
}

/** Returns LAW_ERR_SYS, LAW_ERR_WNTR, LAW_ERR_OK. */
sel_err_t law_task_queue_pop(law_task_queue_t *queue, law_task_t **task)
{
        return law_pipe_write(queue->fds, task, sizeof(void*));
}

/* task node callbacks ################################################### */

static void law_task_set_next(void *task, void *next) 
{
        ((struct law_task*)task)->next = next;
}

static void *law_task_get_next(void *task)
{
        return ((struct law_task*)task)->next;
}

pmt_ll_node_iface_t law_task_lln_iface = {
        .get_next = law_task_get_next,
        .set_next = law_task_set_next
};

/* law_task ############################################################## */

law_task_t *law_task_create(size_t stack_length, size_t stack_guard)
{
        law_task_t *task = malloc(sizeof(law_task_t));
        if(!task) return NULL;

        task->mode = LAW_MODE_CREATED;
        task->version = (law_version_t)rand();
        task->events = NULL;
        task->max_events = 0;
        task->num_events = 0;
        task->callback = (law_callback_t)0;
        task->data = (law_data_t){ .ptr = NULL };
        task->next = NULL;

        if(!(task->callee = law_cor_create()))
                goto FREE_TASK;
      
        if(!(task->stack = law_smem_create(stack_length, stack_guard)))
                goto FREE_COR;

        return task;

        FREE_COR:
        law_cor_destroy(task->callee);

        FREE_TASK:
        free(task);

        return NULL;
}

void law_task_destroy(law_task_t *task) 
{
        if(!task) return;
        law_smem_destroy(task->stack);
        law_cor_destroy(task->callee);
        free(task);
}

law_task_t *law_task_setup(
        law_task_t *task,
        law_id_t id,
        law_callback_t callback,
        law_data_t data)
{
        task->id = id;
        task->callback = callback;
        task->data = data;
        task->version ^= (task->version << 29) | (task->version >> 35);
        task->mode = LAW_MODE_SPAWNED;
        
        return task;
}

/* task pool ############################################################# */

/** Initialize the task pool */
law_task_pool_t *law_task_pool_create(law_server_config_t *cfg)
{
        law_task_pool_t *pool = malloc(sizeof(law_task_pool_t));
        if(!pool) return NULL;

        pool->list = NULL;
        pthread_mutex_init(&pool->lock, NULL);

        int capacity = cfg->worker_tasks * cfg->workers;
        
        pool->capacity = (size_t)capacity;
        pool->size = 0;

        for(int n = 0; n < capacity; ++n) {
                law_task_t *task = law_task_create(cfg->stack, cfg->guards);
                if(!task) goto FAILURE;
                law_task_pool_push(pool, task);
        }

        SEL_ASSERT(pool->capacity == pool->size);

        return pool;

        FAILURE:;

        law_task_t *task = NULL;

        while((task = law_task_pool_pop(pool))) {
                law_task_destroy(task);
        }
 
        free(pool);

        return NULL;
}

void law_task_pool_destroy(law_task_pool_t *pool)
{
        if(!pool) return;
        law_task_t *task = NULL;
        while((task = law_task_pool_pop(pool))) {
                law_task_destroy(task);
        }
        free(pool);
}

/** Push a task to the pool. */
void law_task_pool_push(law_task_pool_t *pool, law_task_t *task)
{
        SEL_ASSERT(pool && task);

        pthread_mutex_lock(&pool->lock);
        
        pool->list = pmt_ll_node_push_front(
                &law_task_lln_iface, 
                pool->list,
                task);

        ++pool->size;

        pthread_mutex_unlock(&pool->lock);
}

/** Pop a task from the pool. */
law_task_t *law_task_pool_pop(law_task_pool_t *pool)
{
        SEL_ASSERT(pool);

        pthread_mutex_lock(&pool->lock);
        
        law_task_t *task = pmt_ll_node_remove_first(
                &law_task_lln_iface, 
                &pool->list);
       
        if(task) --pool->size;

        pthread_mutex_unlock(&pool->lock);

        return task;
}

size_t law_task_pool_size(law_task_pool_t *pool)
{
        pthread_mutex_lock(&pool->lock);
        size_t size = pool->size;
        pthread_mutex_unlock(&pool->lock);
        return size;
}

bool law_task_pool_is_empty(law_task_pool_t *pool)
{
        pthread_mutex_lock(&pool->lock);
        const bool result = pool->list ? false : true;
        pthread_mutex_unlock(&pool->lock);
        return result;
}

bool law_task_pool_is_full(law_task_pool_t *pool) 
{
        pthread_mutex_lock(&pool->lock);
        const bool result = pool->size == pool->capacity;
        pthread_mutex_unlock(&pool->lock);
        return result;
}

/* ready set ############################################################# */

/** Initialize the ready set */
void law_ready_set_init(struct law_ready_set *set)
{
        SEL_ASSERT(set);
        set->list = NULL;
}

/** Add the task to the ready set. */
bool law_ready_set_push(struct law_ready_set *set, struct law_task *task)
{
        SEL_ASSERT(set && task);

        if(task->mark) 
                return false;

        task->mark = 1;

        set->list = pmt_ll_node_push_front(
                &law_task_lln_iface, 
                set->list, 
                task);

        return true;
}

/** Remove the task from the ready set. */
struct law_task *law_ready_set_pop(struct law_ready_set *set)
{
        SEL_ASSERT(set);

        struct law_task *task = pmt_ll_node_remove_first(
                &law_task_lln_iface,
                (void**)&set->list);
        if(!task) 
                return NULL;

        task->mark = 0;

        return task;
}

void law_task_push_event(law_task_t *task, law_event_t *ev)
{
        SEL_ASSERT(task->events && task->num_events <= task->max_events);
        if(task->num_events < task->max_events)
                task->events[task->num_events++] = *ev;
}

/* law_worker ############################################################ */

law_worker_t *law_worker_create(law_server_t *server, const int id)
{
        SEL_ASSERT(server);

        law_worker_t *w = calloc(1, sizeof(law_worker_t));
        if(!w) return NULL;

        w->server = server;
        w->mode = LAW_MODE_CREATED;
        law_ready_set_init(&w->ready);

        if(!(w->caller = law_cor_create()))
                goto FREE_WORKER;

        if(!(w->timer = law_timer_create(server->cfg.worker_tasks)))
                goto FREE_CALLER;
  
        if(!(w->evo = law_evo_create(server->cfg.worker_events)))
                goto FREE_TIMER;

        const size_t table_size = ((server->cfg.worker_tasks * 4) / 3) + 1;

        if(!(w->table = law_table_create(table_size)))
                goto FREE_EVO;

        return w;

        FREE_EVO:
        law_evo_destroy(w->evo);

        FREE_TIMER:
        law_timer_destroy(w->timer);

        FREE_CALLER:
        law_cor_destroy(w->caller);

        FREE_WORKER:
        free(w);

        return NULL;
}

void law_worker_destroy(law_worker_t *w)
{
        if(!w) return;
        law_table_destroy(w->table);
        law_evo_destroy(w->evo);
        law_timer_destroy(w->timer);
        law_cor_destroy(w->caller);
        free(w);
}

law_server_t *law_get_server(law_worker_t *worker) 
{
        return worker->server;
}

sel_err_t law_worker_open(law_worker_t *worker) 
{
        if(law_task_queue_open(&worker->incoming) == LAW_ERR_SYS)
                return LAW_ERR_SYS;

        if(law_msg_queue_open(&worker->messages) == LAW_ERR_SYS) 
                goto CLOSE_INCOMING;

        if(law_evo_open(worker->evo) == -1) 
                goto CLOSE_MESSAGES;

        law_event_t event = { .events = LAW_EV_R, .data = { .ptr = NULL } };

        if(law_evo_ctl(
                worker->evo, 
                worker->incoming.fds[0], 
                LAW_EV_ADD, 
                0, 
                &event) == -1) 
                goto CLOSE_EVO;
        
        if(law_evo_ctl(
                worker->evo,
                worker->messages.fds[0],
                LAW_EV_ADD,
                0,
                &event) == -1)
                goto CLOSE_EVO;
        
        return LAW_ERR_OK;

        CLOSE_EVO:
        law_evo_close(worker->evo);

        CLOSE_MESSAGES:
        law_msg_queue_close(&worker->messages);

        CLOSE_INCOMING:
        law_task_queue_close(&worker->incoming);

        return LAW_ERR_SYS;
}

void law_worker_close(struct law_worker *worker) 
{
        law_evo_close(worker->evo);
        law_msg_queue_close(&worker->messages);
        law_task_queue_close(&worker->incoming);
        return LAW_ERR_OK;
}

/* law_server ############################################################ */

law_server_t *law_server_create(law_server_config_t *cfg)
{
        SEL_ASSERT(cfg);

        const int nthreads = cfg->workers;
        const int nevents = cfg->server_events;
        SEL_ASSERT(0 < nthreads && nthreads < 0x1000);
        SEL_ASSERT(0 < nevents && nevents < 0x1000);

        struct law_server *s = calloc(1, sizeof(struct law_server));
        if(!s) return NULL;

        s->cfg = *cfg;
        s->mode = LAW_MODE_CREATED;
        s->socket = 0;
        s->seed = 0;

        if(!(s->threads = calloc((size_t)nthreads, sizeof(pthread_t))))
                goto FREE_SERVER;

        if(!(s->workers = calloc((size_t)nthreads, sizeof(void*))))
                goto FREE_THREADS;

        if(!(s->evo = law_evo_create(cfg->server_events))) 
                goto FREE_WORKERS;

        if(!(s->pool = law_task_pool_create(cfg)))
                goto FREE_EVO;

        int n_threads = 0;

        for(; n_threads < nthreads; ++n_threads) {
                s->workers[n_threads] = law_worker_create(s, n_threads);
                if(!s->workers[n_threads]) goto FREE_THREADS;
        }

        return s;

        FREE_THREADS:

        for(int m = 0; m < n_threads; ++m) {
                law_worker_destroy(s->workers[m]);
        }

        law_task_pool_destroy(s->pool);

        FREE_EVO:
        law_evo_destroy(s->evo);

        FREE_WORKERS:
        free(s->workers);

        FREE_THREADS:
        free(s->threads);

        FREE_SERVER:
        free(s);

        return NULL;
}

void law_server_destroy(law_server_t *srv) 
{       
        if(!srv) return;
        for(int n = 0; n < srv->cfg.workers; ++n) {
                law_worker_destroy(srv->workers[n]);
        }
        law_task_pool_destroy(srv->pool);
        law_evo_destroy(srv->evo);
        free(srv->workers);
        free(srv->threads);
        free(srv);
}

law_id_t law_get_active(law_worker_t *worker)
{
        return worker->active->id;
}

int law_get_socket(law_server_t *server)
{
        return server->socket;
}

volatile law_id_t law_server_genid(law_server_t *server)
{
        return (law_id_t)atomic_fetch_add(&server->seed, 1);
}

sel_err_t law_open(law_server_t *server)
{
        SEL_ASSERT(server && server->socket);

        sel_err_t error = LAW_ERR_SYS;
        int n = 0; 

        if(law_evo_open(server->evo) == -1) 
                return LAW_ERR_SYS;

        law_event_t event = { .events = LAW_EV_R, .data = { .ptr = NULL } };
        if(law_evo_ctl(
                server->evo, 
                server->socket, 
                LAW_EV_ADD, 
                0, 
                &event) == -1) 
                goto CLOSE_EVO;

        law_worker_t **ws = server->workers;

        for(; n < server->cfg.workers; ++n) {
                if((error = law_worker_open(ws[n])) != LAW_ERR_OK) {
                        goto CLOSE_WORKERS;
                }
        }

        CLOSE_WORKERS:
        for(int m = 0; m < n; ++m) {
                law_worker_close(ws[m]);
        }

        CLOSE_EVO:
        law_evo_close(server->evo);

        return error;
}

sel_err_t law_close(law_server_t *server)
{
        if(!server) return SEL_ERR_OK;
        for(int n = 0; n < server->cfg.workers; ++ n) {
                law_worker_close(server->workers[n]);
        }
        law_evo_close(server->evo);
        close(server->socket);
        return SEL_ERR_OK;
}

sel_err_t law_ectl(
        law_worker_t *worker,
        law_slot_t *slot,
        int op,
        law_event_bits_t flags,
        law_event_bits_t events)
{
        SEL_ASSERT(worker && slot);
        struct law_event ev = { .events = events, .data = { .ptr = slot } };
        return law_evo_ctl(worker->evo, slot->fd, op, flags, &ev);
}

int law_ewait(
        law_worker_t *w,
        law_time_t expiration,
        law_event_t *events,
        int max_events)
{
        SEL_ASSERT(w && w->active);

        law_task_t *task = w->active;
        
        if(expiration <= law_time_millis()) 
                return LAW_ERR_TTL;

        if(!law_timer_insert(w->timer, expiration, task->id, task->version))
                return LAW_ERR_OOM;

        task->max_events = max_events;
        task->num_events = 0;
        task->events = events;

        (void)law_cor_yield(w->caller, task->callee, LAW_MODE_SUSPENDED);
        
        int num_events = task->num_events;

        task->max_events = 0;
        task->num_events = 0;
        task->events = NULL;

        return num_events;
}

static bool law_spawn_dispatch_once(law_server_t *s, law_task_t *task)
{
        int num_workers = s->cfg.workers;
        law_worker_t **ws = s->workers;

        for(int x = 0; x < num_workers; ++x) {
                switch(law_task_queue_push(&ws[x]->incoming, task)) {
                        case LAW_ERR_WNTW: 
                                continue; 
                        case LAW_ERR_OK: 
                                return true;
                        default: 
                                SEL_HALT();
                }
        }

        law_event_t event = { .events = 0, .data = { .ptr = NULL } };
        SEL_TEST(law_evo_ctl(s->evo, s->socket, LAW_EV_DEL, 0, &event) == 0);
        
        event.events = LAW_EV_W;
        for(int x = 0; x < num_workers; ++x) {
                SEL_TEST(law_evo_ctl(
                        s->evo, 
                        ws[x]->incoming.fds[1], 
                        LAW_EV_ADD, 
                        0, 
                        &event) == 0);
        }

        SEL_TEST(law_evo_wait(s->evo, 100) != -1);

        event.events = LAW_EV_R;
        SEL_TEST(law_evo_ctl(s->evo, s->socket, LAW_EV_ADD, 0, &event) == 0);

        event.events = 0;
        for(int x = 0; x < num_workers; ++x) {
                SEL_TEST(law_evo_ctl(
                        s->evo, 
                        ws[x]->incoming.fds[1], 
                        LAW_EV_DEL, 
                        0, 
                        &event) == 0);
        }

        return false;
}

static void law_spawn_dispatch(law_server_t *server, law_task_t *task)
{
        while(!law_spawn_dispatch_once(server, task));
}

sel_err_t law_spawn(
        law_server_t *server,
        law_callback_t callback,
        law_data_t data)
{
        SEL_ASSERT(server && callback);

        law_task_t *task = law_task_pool_pop(server->pool);

        if(!task) return LAW_ERR_LIM;

        law_task_setup(task, law_server_genid(server), callback, data);

        law_worker_t **ws = server->workers;

        for(int x = 0; x < server->cfg.workers; ++x) {
                switch(law_task_queue_push(&ws[x]->incoming, task)) {
                        case LAW_ERR_WNTW: 
                                continue; 
                        case LAW_ERR_OK: 
                                return LAW_ERR_OK;
                        default: 
                                SEL_HALT();
                }
        }

        return LAW_ERR_WNTW;
}

static int law_task_cor_trampoline(
        law_cor_t *init_env, 
        law_cor_t *cor_env, 
        void *state)
{
        law_worker_t *worker = state;
        law_task_t *task = worker->active;
        return task->callback(worker, task->data);
}

static void law_worker_dispatch(law_worker_t *w)
{
        struct law_task *task = NULL;

        while((task = law_ready_set_pop(&w->ready))) {

                SEL_ASSERT(task && task->next == NULL && !task->mark);

                ++task->version;

                sel_err_t signal = -1;

                switch(task->mode) {
                        case LAW_MODE_SPAWNED:
                                signal = law_cor_call(
                                        w->caller, 
                                        task->callee, 
                                        task->stack,
                                        law_task_cor_trampoline,
                                        w);
                                break;
                        case LAW_MODE_SUSPENDED:
                                signal = law_cor_resume(
                                        w->caller,
                                        task->callee,
                                        LAW_ERR_OK);
                                break;
                        default: 
                                SEL_HALT();
                }

                if(signal == LAW_MODE_SUSPENDED) 
                        continue;

                SEL_TEST(law_task_map_remove(w->table, task->id) == task);
                
                law_task_pool_push(w->server->pool, task);

                task = NULL;
        }
}

static bool law_worker_tick(law_worker_t *worker)
{
        law_server_t *server = worker->server;
        law_timer_t *timer = worker->timer;
        law_ready_set_t *ready = &worker->ready;
        law_table_t *table = worker->table;

        law_time_t 
                timeout = server->cfg.worker_timeout,
                min_expiry = 0,
                now = 0;

        bool reloop = true;

        if(law_timer_peek(timer, &min_expiry, NULL, NULL)) {
                now = law_time_millis();
                if(min_expiry <= now) {
                        timeout = 0;
                } else {
                        law_time_t wake = min_expiry - now;
                        timeout = timeout < wake ? timeout : wake;
                }
        }
       
        SEL_TEST(law_evo_wait(worker->evo, (int)timeout) >= 0);

        law_version_t version = 0;
        law_id_t id = 0;
        law_event_t event = { .events = LAW_EV_TIM, .data = { .ptr = NULL } };

        now = law_time_millis();

        while(law_timer_peek(timer, &min_expiry, &id, &version)) {

                if(min_expiry > now) break;

                SEL_TEST(law_timer_pop(timer, NULL, NULL, NULL));

                law_task_t *task = law_table_lookup(table, id);
                if(!task || task->version != version) continue;

                (void)law_task_push_event(task, &event);
                (void)law_ready_set_push(ready, task);

                min_expiry = id = version = 0;
        }

        while(law_evo_next(worker->evo, &event)) {

                law_slot_t *slot = event.data.ptr;
                if(!slot) continue;

                law_task_t *task = law_table_lookup(
                        worker->table, 
                        slot->id);
                if(!task) continue;
                
                (void)law_task_push_event(task, &event);
                (void)law_ready_set_push(ready, task);

                event.events = event.data.u64 = 0;
        }

        law_msg_t msg = { .type = 0, .data = { .ptr = NULL } };
        
        event.events = LAW_EV_WAK;
        event.data.ptr = NULL;

        while(law_msg_queue_pop(&worker->messages, &msg)) {
                if(msg.type == LAW_MSG_SHUTDOWN) {

                        reloop = false;

                } else if(msg.type == LAW_MSG_WAKEUP) {

                        law_task_t *task = law_table_lookup(
                                worker->table, 
                                msg.data.u64);
                        if(!task) continue;

                        (void)law_task_push_event(task, &event);
                        (void)law_ready_set_push(ready, task);

                } else {
                        SEL_HALT();
                }
                
                msg.type = msg.data.u64 = 0;
        }

        size_t max_size = law_table_max_size(table);

        while(law_table_size(table) <= max_size) {

                law_task_t *task = NULL;
                if(!law_task_queue_pop(&worker->incoming, &task)) 
                        break;
                
                SEL_ASSERT(task && task->mode == LAW_MODE_SPAWNED);

                SEL_TEST(law_table_insert(
                        table, 
                        task->id, 
                        task) == PMT_HM_SUCCESS);

                (void)law_ready_set_push(ready, task);
        }
        
        (void)law_worker_dispatch(worker);
        
        return reloop;
}

static void law_worker_run(struct law_worker *w)
{
        if(w->server->cfg.init(w, w->server->cfg.data) != LAW_ERR_OK) 
                return;

        while(law_worker_tick(w));
}

static void *law_worker_thread(void *state) 
{
        (void)law_worker_run((law_worker_t*)state);
        return NULL;
}

static sel_err_t law_accept_callback(law_worker_t *worker, law_data_t data)
{
        law_server_t *s = worker->server;
        return s->cfg.accept(worker, data.fd, s->cfg.data);
}

static sel_err_t law_server_accept(law_server_t *server) 
{
        while(!law_task_pool_is_empty(server->pool)) {

                int fd = accept(server->socket, NULL, NULL);
                if(fd == -1) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) 
                                return LAW_ERR_OK;
                        switch(errno) {
                                case EHOSTUNREACH:
                                case ENETUNREACH:
                                case ENONET:
                                case EHOSTDOWN:
                                case ENETDOWN:
                                        goto WAIT;
                                case EPROTO:
                                case ENOPROTOOPT:
                                case ECONNABORTED:
                                case EOPNOTSUPP:
                                        continue;
                                default:
                                        return LAW_ERR_SYS;
                        }
                }

                int flags = fcntl(fd, F_GETFL);
                if(flags != -1) {
                        close(fd);
                        return SEL_ERR_SYS;       
                }

                if(fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) {
                        close(fd);
                        return SEL_ERR_SYS;  
                }

                law_task_t *task = law_task_pool_pop(server->pool);
                SEL_TEST(task);

                (void)law_task_setup(
                        task, 
                        law_server_genid(server), 
                        law_accept_callback, 
                        server->cfg.data);
                
                (void)law_spawn_dispatch(server, task);
        }

        WAIT:

        law_time_sleep(75);

        return LAW_ERR_OK;
}

static sel_err_t law_server_spin(law_server_t *s)
{
        sel_err_t error = LAW_ERR_OK;

        while(s->mode == LAW_MODE_RUNNING) {

                error = law_server_accept(s);
                if(error != LAW_ERR_OK) break;

                SEL_TEST(law_evo_wait(s->evo, s->cfg.server_timeout) >= 0);
        }

        while(!law_task_pool_is_full(s->pool)) {
                law_time_sleep(50);
        }

        law_msg_t msg = { .type = LAW_MSG_SHUTDOWN, .data = { .u64 = 0 } };

        for(int n = 0; n < s->cfg.workers; ++n) {
                law_msg_queue_push(s->workers[n], &msg);
        }

        return error;
}

static sel_err_t law_server_run_threads(law_server_t *s)
{
        SEL_ASSERT(s && s->workers && s->threads);
        
        law_worker_t **workers = s->workers;
        pthread_t *threads = s->threads;
        int nthreads = s->cfg.workers;

        for(int n = 0; n < nthreads; ++n) {
                law_worker_t *worker = workers[n];
                SEL_ASSERT(worker && worker->server == s);
                SEL_TEST(pthread_create(
                        threads + n,
                        NULL, 
                        law_worker_thread, 
                        worker) == 0);
        }

        sel_err_t error = law_server_spin(s);

        for(int m = 0; m < nthreads; ++m) {
                pthread_join(threads[m], NULL);
        }

        return error;
}

sel_err_t law_start(law_server_t *s)
{
        if(s->mode != LAW_MODE_CREATED) 
                return LAW_ERR_MODE;

        s->mode = LAW_MODE_RUNNING;

        sel_err_t error = law_server_run_threads(s);
        
        s->mode = LAW_MODE_STOPPED;
        
        return error;
}

sel_err_t law_stop(law_server_t *s)
{
        s->mode = LAW_MODE_STOPPING;
        return LAW_ERR_OK;
}

/* networking ############################################################ */

sel_err_t law_create_socket(law_server_t *server, int *socket_fd)
{
        SEL_ASSERT(server);

        int     domain = -1, 
                type = -1;
        
        switch(server->cfg.protocol) {
                case LAW_PROTOCOL_TCP: 
                        domain = AF_INET;
                        type = SOCK_STREAM;
                        break;
                case LAW_PROTOCOL_TCP6:
                        domain = AF_INET6;
                        type = SOCK_STREAM;
                        break;
                case LAW_PROTOCOL_UDP:
                        domain = AF_INET;
                        type = SOCK_DGRAM;
                        break;
                case LAW_PROTOCOL_UDP6:
                        domain = AF_INET6;
                        type = SOCK_DGRAM;
                        break;
                default: 
                        SEL_HALT();
        }

        const int fd = socket(domain, type, 0);
        if(fd == -1) return LAW_ERR_SYS;
        
        const int flags = fcntl(fd, F_GETFL);
        if(flags == -1) goto FAILURE;
        if(fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) goto FAILURE;
        
        server->socket = fd;
        if(*socket_fd) *socket_fd = fd;

        FAILURE:

        close(fd);

        return SEL_ERR_SYS;
}

sel_err_t law_bind(law_server_t *server)
{
        SEL_ASSERT(server);

        struct sockaddr_storage addr;
        memset(&addr, 0, sizeof(struct sockaddr_storage));
        struct sockaddr_in *in = NULL;
        struct sockaddr_in6 *in6 = NULL;
        unsigned int addrlen = -1;

        switch(server->cfg.protocol) {
                case LAW_PROTOCOL_TCP: 
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons(server->cfg.port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                        break;
                case LAW_PROTOCOL_TCP6:
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons(server->cfg.port);
                        in6->sin6_addr = in6addr_any;
                        break;
                case LAW_PROTOCOL_UDP:
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons(server->cfg.port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                        break;
                case LAW_PROTOCOL_UDP6:
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons(server->cfg.port);
                        in6->sin6_addr = in6addr_any;
                        break;
                default: 
                        SEL_HALT();
        }

        SEL_ASSERT(server->socket);

        if(bind(server->socket, (struct sockaddr*)&addr, addrlen) == -1) 
                return SEL_ERR_SYS; 
        
        return SEL_ERR_OK;
}

sel_err_t law_listen(law_server_t *server)
{
        SEL_ASSERT(server && server->socket);

        if(listen(server->socket, server->cfg.backlog) < 0) 
                return SEL_ERR_SYS;
        
        return SEL_ERR_OK;
}

sel_err_t law_drop_root_privileges(law_server_t *server)
{
        SEL_ASSERT(server);

        if(setgid(server->cfg.gid) == -1) return SEL_ERR_SYS;
        if(setuid(server->cfg.uid) == -1) return SEL_ERR_SYS;

        return SEL_ERR_OK;
}
