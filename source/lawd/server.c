/** server.h implementation */

/* See man getaddrinfo */
#define _POSIX_C_SOURCE 200112L

#include "lawd/error.h"
#include "lawd/server.h"
#include "lawd/coroutine.h"
#include "lawd/safemem.h"
#include "lawd/table.h"
#include "lawd/private/server.h"

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <math.h>

law_server_cfg_t law_server_sanity()
{
        law_server_cfg_t cfg;

        cfg.protocol            = LAW_PROTOCOL_TCP;
        cfg.port                = 80;
        cfg.backlog             = 8;

        cfg.workers             = 1;  
        cfg.worker_tasks        = 4;

        cfg.worker_timeout      = 5000;
        cfg.server_timeout      = 5000;

        cfg.stack               = 0x100000;
        cfg.guards              = 0x1000;
        
        cfg.server_events       = 32;
        cfg.worker_events       = 32;
        
        cfg.on_error            = NULL;
        cfg.on_accept           = NULL;

        cfg.data                = (law_data_t){ .u64 = 0 };
        
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
        law_vers_t version;                     /** Version Number */
        law_id_t id;                            /** Task Identifier */
        law_event_t *events;                    /** User's Event Array */
        struct law_task *next;                  /** Next Task */
        law_worker_t *worker;                   /** Task's Worker */
        law_cor_t *callee;                      /** Coroutine Callee */
        law_smem_t *stack;                      /** Coroutine Stack */
        law_callback_t callback;                /** User Callback */
        law_data_t data;                        /** User Data */
        int slots[16];                          /** I/O Slots */
} law_task_t;

enum law_msg_type {                             /** Message Type */
        LAW_MSG_SHUTDOWN        = 1,            /** Prepare for Shutdown */
        LAW_MSG_WAKEUP          = 2             /** Wakeup Notification */
};

struct law_worker {
        int id;
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

struct law_server {
        law_server_cfg_t cfg;
        int socket;
        int mode;
        law_id_t seed;
        pthread_mutex_t lock;
        law_task_pool_t *pool;
        pthread_t *threads;
        law_worker_t **workers;
        law_evo_t *evo;
};

#define LAW_ID_MODULO 0x100000000000000

/** 
 * A slot consists of a 56bit task id along with 8bits of user data.  The id
 * portion is 7 bytes encoded in base_256 in "little endian" order like so:
 * 
 *      (uint8_t[8]) { user_data, id_low_bits, ..., id_high_bits }
 * 
 * Despite slots being represented with little endian ordering, this function 
 * along with law_slot_decode should be fully portable between architectures. 
 */
uint64_t law_slot_encode(law_slot_t *slot) 
{
        SEL_ASSERT(0 <= slot->id && slot->id < LAW_ID_MODULO);

        uint64_t result = 0;
        uint8_t *bytes = (uint8_t*)&result;

        bytes[0] = (uint8_t)slot->data;

        uint64_t id = slot->id;

        for(int x = 1; x < 8; ++x) {
                bytes[x] = (uint8_t)(id % 0x100UL);
                id /= 0x100UL;
        }

        return result;
}

/**
 * Decode the encoded slot.
 */
void law_slot_decode(uint64_t encoding, law_slot_t *slot)
{
        SEL_ASSERT(slot);

        uint8_t *bytes = (uint8_t*)&encoding;
        slot->data = (int8_t)bytes[0];

        uint64_t id = 0;
        uint64_t pow = 1;

        for(uint64_t x = 0; x < 7; ++x) {
                id += bytes[x + 1] * pow;
                pow *= 0x100UL;
        }
        
        slot->id = id;
}

/** Close a pipe. */
static void law_pipe_close(int fds[2])
{
        close(fds[0]);
        close(fds[1]);
}

/** Open a non-blocking pipe. */
static int law_pipe_open(int fds[2])
{
        if(pipe(fds) == -1) 
                return LAW_ERR_PUSH(LAW_ERR_SYS, "pipe");

        int flags;

        if((flags = fcntl(fds[0], F_GETFL)) == -1) {
                law_pipe_close(fds);
                return LAW_ERR_PUSH(LAW_ERR_SYS, "fcntl");
        }

        if(fcntl(fds[0], F_SETFL, O_NONBLOCK | flags) == -1) {
                law_pipe_close(fds);
                return LAW_ERR_PUSH(LAW_ERR_SYS, "fcntl");
        }

        if((flags = fcntl(fds[1], F_GETFL)) == -1) {
                law_pipe_close(fds);
                return LAW_ERR_PUSH(LAW_ERR_SYS, "fcntl");
        }

        if(fcntl(fds[1], F_SETFL, O_NONBLOCK | flags) == -1) {
                law_pipe_close(fds);
                return LAW_ERR_PUSH(LAW_ERR_SYS, "fcntl");
        }
        
        return LAW_ERR_OK;
}

/** Write nbytes of addr to the pipe. */
static sel_err_t law_pipe_write(int fd, void *addr, size_t nbytes)
{
        const ssize_t result = write(fd, addr, nbytes);

        if(result == -1) {
                if(errno == EWOULDBLOCK || errno == EAGAIN) 
                        return LAW_ERR_WANTW;
                else 
                        return LAW_ERR_SYS;
        } 

        SEL_TEST(result == nbytes);

        return LAW_ERR_OK;
}

/** Read exactly nbytes to address. */
static sel_err_t law_pipe_read(int fd, void *addr, size_t nbytes)
{
        const ssize_t result = read(fd, addr, nbytes);

        if(result == -1) {
                if(errno == EWOULDBLOCK || errno == EAGAIN)
                        return LAW_ERR_WANTR;
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
        return law_pipe_write(queue->fds[1], msg, sizeof(law_msg_t));
}

/** Returns LAW_ERR_SYS, LAW_ERR_WNTR, LAW_ERR_OK.*/
sel_err_t law_msg_queue_pop(law_msg_queue_t *queue, law_msg_t *msg)
{
        return law_pipe_read(queue->fds[0], msg, sizeof(law_msg_t));
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
        return law_pipe_write(queue->fds[1], &task, sizeof(void*));
}

/** Returns LAW_ERR_SYS, LAW_ERR_WNTR, LAW_ERR_OK. */
sel_err_t law_task_queue_pop(law_task_queue_t *queue, law_task_t **task)
{
        return law_pipe_read(queue->fds[0], task, sizeof(void*));
}

/* task node callbacks ################################################### */

static void law_task_set_next(void *task, void *next) 
{
        ((law_task_t*)task)->next = next;
}

static void *law_task_get_next(void *task)
{
        return ((law_task_t*)task)->next;
}

pmt_ll_node_iface_t law_task_lln_iface = {
        .get_next = law_task_get_next,
        .set_next = law_task_set_next
};

/* law_task ############################################################## */

law_task_t *law_task_create(size_t stack_length, size_t stack_guard)
{
        law_task_t *task = calloc(1, sizeof(law_task_t));
        if(!task) return NULL;

        task->mode = LAW_MODE_CREATED;
        task->version = (law_vers_t)rand();
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
        task->version = (law_vers_t)rand();
        task->mode = LAW_MODE_SPAWNED;
        return task;
}

/* task pool ############################################################# */

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
        
        if(!pool->list) {
                SEL_ASSERT(pool->size == 0);
                return NULL;
        }

        law_task_t *task = pmt_ll_node_remove_first(
                &law_task_lln_iface, 
                (void**)&pool->list);
       
        SEL_ASSERT(task);

        --pool->size;

        pthread_mutex_unlock(&pool->lock);

        return task;
}

/** Initialize the task pool */
law_task_pool_t *law_task_pool_create(law_server_cfg_t *cfg)
{
        law_task_pool_t *pool = calloc(1, sizeof(law_task_pool_t));
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
void law_ready_set_init(law_ready_set_t *set)
{
        SEL_ASSERT(set);
        memset(set, 0, sizeof(law_ready_set_t));
}

/** Add the task to the ready set. */
void law_ready_set_push(law_ready_set_t *set, law_task_t *task)
{
        SEL_ASSERT(set && task);

        if(task->mark) return;

        task->mark = 1;

        set->list = pmt_ll_node_push_front(
                &law_task_lln_iface, 
                set->list, 
                task);
}

/** Remove the task from the ready set. */
law_task_t *law_ready_set_pop(law_ready_set_t *set)
{
        SEL_ASSERT(set);

        law_task_t *task = pmt_ll_node_remove_first(
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

        w->id = id;
        w->server = server;
        w->mode = LAW_MODE_CREATED;
        law_ready_set_init(&w->ready);

        if(!(w->caller = law_cor_create()))
                goto FREE_WORKER;

        if(!(w->timer = law_timer_create((size_t)server->cfg.worker_tasks)))
                goto FREE_CALLER;
  
        if(!(w->evo = law_evo_create(server->cfg.worker_events)))
                goto FREE_TIMER;

        size_t table_size = (size_t)((server->cfg.worker_tasks * 4) / 3) + 2;

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
                return LAW_ERR_PUSH(LAW_ERR_SYS, "law_task_queue_open");

        if(law_msg_queue_open(&worker->messages) == LAW_ERR_SYS) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_msg_queue_open");
                goto CLOSE_INCOMING;
        }
                
        if(law_evo_open(worker->evo) == -1) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_evo_open");
                goto CLOSE_MESSAGES;
        }

        law_event_t event = { .events = LAW_EV_R, .data = { .ptr = NULL } };

        if(law_evo_ctl(
                worker->evo, 
                worker->incoming.fds[0], 
                LAW_EV_ADD, 
                0, 
                &event) == -1) 
        {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_evo_ctl");
                goto CLOSE_EVO;
        }

        if(law_evo_ctl(
                worker->evo,
                worker->messages.fds[0],
                LAW_EV_ADD,
                0,
                &event) == -1)
        {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_evo_ctl");
                goto CLOSE_EVO;
        }
        
        return LAW_ERR_OK;

        CLOSE_EVO:
        law_evo_close(worker->evo);

        CLOSE_MESSAGES:
        law_msg_queue_close(&worker->messages);

        CLOSE_INCOMING:
        law_task_queue_close(&worker->incoming);

        return LAW_ERR_SYS;
}

void law_worker_close(law_worker_t *worker) 
{
        law_evo_close(worker->evo);
        law_msg_queue_close(&worker->messages);
        law_task_queue_close(&worker->incoming);
}

/* law_server ############################################################ */

law_server_t *law_server_create(law_server_cfg_t *cfg)
{
        SEL_ASSERT(cfg);

        const int nthreads = cfg->workers;
        const int nevents = cfg->server_events;
        SEL_ASSERT(0 < nthreads && nthreads < 0x1000);
        SEL_ASSERT(0 < nevents && nevents < 0x1000);

        law_server_t *s = calloc(1, sizeof(law_server_t));
        if(!s) return NULL;

        s->cfg = *cfg;
        s->mode = LAW_MODE_CREATED;
        s->socket = 0;
        s->seed = 0;
        pthread_mutex_init(&s->lock, NULL);

        if(!(s->threads = calloc((size_t)nthreads, sizeof(pthread_t))))
                goto FREE_SERVER;

        if(!(s->workers = calloc((size_t)nthreads, sizeof(void*))))
                goto FREE_PTHREADS;

        if(!(s->evo = law_evo_create(cfg->server_events))) 
                goto FREE_WORKER_PTRS;

        if(!(s->pool = law_task_pool_create(cfg)))
                goto FREE_EVO;

        int n = 0;

        for(; n < nthreads; ++n) {
                s->workers[n] = law_worker_create(s, n);
                if(!s->workers[n]) goto FREE_WORKERS;
        }

        return s;

        FREE_WORKERS:

        for(int m = 0; m < n; ++m) {
                law_worker_destroy(s->workers[m]);
        }

        law_task_pool_destroy(s->pool);

        FREE_EVO:
        law_evo_destroy(s->evo);

        FREE_WORKER_PTRS:
        free(s->workers);

        FREE_PTHREADS:
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

law_id_t law_get_active_id(law_worker_t *worker)
{
        return worker->active->id;
}

int law_get_worker_id(law_worker_t *worker) 
{
        return worker->id;
}

int law_get_server_socket(law_server_t *server)
{
        return server->socket;
}

law_id_t law_server_genid(law_server_t *server)
{
        pthread_mutex_lock(&server->lock);

        server->seed = (server->seed + 1) % LAW_ID_MODULO;

        law_id_t id = server->seed;

        pthread_mutex_unlock(&server->lock);

        return id;
}

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
        if(fd == -1) 
                return LAW_ERR_PUSH(LAW_ERR_SYS, "socket");
        
        const int flags = fcntl(fd, F_GETFL);
        if(flags == -1) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "fcntl");
                goto FAILURE;
        }
        if(fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "fcntl");
                goto FAILURE;
        }
        
        server->socket = fd;
        if(socket_fd) *socket_fd = fd;

        return SEL_ERR_OK;

        FAILURE:

        close(fd);

        return SEL_ERR_SYS;
}

sel_err_t law_bind_socket(law_server_t *server)
{
        SEL_ASSERT(server);

        struct sockaddr_storage addr;
        memset(&addr, 0, sizeof(struct sockaddr_storage));
        struct sockaddr_in *in = NULL;
        struct sockaddr_in6 *in6 = NULL;
        unsigned int addrlen;

        switch(server->cfg.protocol) {
                case LAW_PROTOCOL_TCP: 
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons((uint16_t)server->cfg.port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                        break;
                case LAW_PROTOCOL_TCP6:
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons((uint16_t)server->cfg.port);
                        in6->sin6_addr = in6addr_any;
                        break;
                case LAW_PROTOCOL_UDP:
                        addrlen = sizeof(struct sockaddr_in);
                        in = (struct sockaddr_in*)&addr;
                        in->sin_family = AF_INET;
                        in->sin_port = htons((uint16_t)server->cfg.port);
                        in->sin_addr.s_addr = htonl(INADDR_ANY);
                        break;
                case LAW_PROTOCOL_UDP6:
                        addrlen = sizeof(struct sockaddr_in6);
                        in6 = (struct sockaddr_in6*)&addr;
                        in6->sin6_family = AF_INET6;
                        in6->sin6_port = htons((uint16_t)server->cfg.port);
                        in6->sin6_addr = in6addr_any;
                        break;
                default: 
                        SEL_HALT();
        }

        SEL_ASSERT(server->socket);

        if(bind(server->socket, (struct sockaddr*)&addr, addrlen) == -1) 
                return LAW_ERR_PUSH(LAW_ERR_SYS, "bind");
        
        return LAW_ERR_OK;
}

sel_err_t law_listen(law_server_t *server)
{
        SEL_ASSERT(server && server->socket);

        if(listen(server->socket, server->cfg.backlog) == -1) 
        return LAW_ERR_PUSH(LAW_ERR_SYS, "listen");
        
        return SEL_ERR_OK;
}

sel_err_t law_open(law_server_t *server)
{
        SEL_ASSERT(server && server->socket);

        sel_err_t error = LAW_ERR_SYS;
        int n = 0; 

        int socket = 0;

        law_err_clear();

        if(law_create_socket(server, &socket) == LAW_ERR_SYS) 
                return LAW_ERR_PUSH(LAW_ERR_SYS, "law_create_socket");

        if(law_bind_socket(server) == LAW_ERR_SYS) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_bind_socket");
                goto CLOSE_SOCKET;
        }

        if(law_listen(server) == LAW_ERR_SYS) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_listen");
                goto CLOSE_SOCKET;
        }

        if(law_evo_open(server->evo) == -1) {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_evo_open");
                goto CLOSE_SOCKET;
        }

        law_event_t event = { .events = LAW_EV_R, .data = { .ptr = NULL } };
        if(law_evo_ctl(
                server->evo, 
                server->socket, 
                LAW_EV_ADD, 
                0, 
                &event) == -1) 
        {
                LAW_ERR_PUSH(LAW_ERR_SYS, "law_evo_ctl");
                goto CLOSE_EVO;
        }
                

        law_worker_t **ws = server->workers;

        for(; n < server->cfg.workers; ++n) {
                if((error = law_worker_open(ws[n])) != LAW_ERR_OK) {
                        goto CLOSE_WORKERS;
                }
        }

        return LAW_ERR_OK;

        CLOSE_WORKERS:
        for(int m = 0; m < n; ++m) {
                law_worker_close(ws[m]);
        }

        CLOSE_EVO:
        law_evo_close(server->evo);

        CLOSE_SOCKET:
        close(socket);

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
        int fd,
        int op,
        law_event_bits_t flags,
        law_event_bits_t events,
        int8_t data)
{
        SEL_ASSERT(worker);

        law_task_t *task = worker->active;
        law_slot_t slot = { 
                .id = task->id, 
                .data = data };

        law_event_t event = { 
                .events = events, 
                .data = { .u64 = law_slot_encode(&slot) } };

        if(law_evo_ctl(
                worker->evo, 
                fd, 
                op, 
                flags, 
                &event) == -1) 
        {
                return LAW_ERR_SYS;
        }

        return LAW_ERR_OK;
}

int law_ewait(
        law_worker_t *w,
        law_time_t millis,
        law_event_t *events,
        int max_events)
{
        SEL_ASSERT(w && w->active && 0 <= millis);

        law_task_t *task = w->active;
        
        law_time_t expiry = law_time_millis() + millis;

        if(!law_timer_insert(w->timer, expiry, task->id, task->version))
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

        for(size_t x = 0; x < num_workers; ++x) {
                const size_t index = (x + task->id) % (size_t)num_workers;
                switch(law_task_queue_push(&ws[index]->incoming, task)) {
                        case LAW_ERR_WANTW: 
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

        if(!task) return LAW_ERR_LIMIT;

        law_task_setup(task, law_server_genid(server), callback, data);

        law_worker_t **ws = server->workers;

        for(int x = 0; x < server->cfg.workers; ++x) {
                switch(law_task_queue_push(&ws[x]->incoming, task)) {
                        case LAW_ERR_WANTW: 
                                continue; 
                        case LAW_ERR_OK: 
                                return LAW_ERR_OK;
                        default: 
                                SEL_HALT();
                }
        }

        return LAW_ERR_WANTW;
}

static sel_err_t law_sync_wait(
        law_worker_t *w,
        law_time_t timeout,
        int fd,
        law_event_bits_t evs)
{
        sel_err_t err = -1;

        if((err = law_ectl(w, fd, LAW_EV_MOD, 0, evs, 0)) != LAW_ERR_OK) {
                return LAW_ERR_PUSH(err, "law_ectl");
        }

        if((err = law_ewait(w, timeout, NULL, 0)) == LAW_ERR_TIME) {
                return LAW_ERR_PUSH(err, "law_ewait");
        } 

        return LAW_ERR_OK;
}

sel_err_t law_sync(
        law_worker_t *worker,
        law_time_t timeout,
        sel_err_t (*callback)(int fd, void *state),
        int fd,
        void *state)
{
        law_err_clear();

        sel_err_t err = -1;

        const law_time_t expiry = law_time_millis() + timeout;
       
        while((err = callback(fd, state)) != LAW_ERR_OK) {

                const law_time_t now = law_time_millis();
                if(expiry <= now) {
                        return LAW_ERR_TIME;
                } 
                
                timeout = expiry - now;

                if(err == LAW_ERR_WANTW) {
                        err = law_sync_wait(worker, timeout, fd, LAW_EV_W);
                } else if(err == LAW_ERR_WANTR) {
                        err = law_sync_wait(worker, timeout, fd, LAW_EV_R);
                } else {
                        return err;
                }

                if(err != LAW_ERR_OK) {
                        return err;
                }
        }

        return LAW_ERR_OK;
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
        law_task_t *task = NULL;

        while((task = law_ready_set_pop(&w->ready))) {

                SEL_ASSERT(task && task->next == NULL && !task->mark);

                ++task->version;
                w->active = task;

                sel_err_t signal = -1;

                switch(task->mode) {
                        case LAW_MODE_SPAWNED:
                                task->mode = LAW_MODE_RUNNING;
                                signal = law_cor_call(
                                        w->caller, 
                                        task->callee, 
                                        task->stack,
                                        law_task_cor_trampoline,
                                        w);
                                break;
                        case LAW_MODE_SUSPENDED:
                                task->mode = LAW_MODE_RUNNING;
                                signal = law_cor_resume(
                                        w->caller,
                                        task->callee,
                                        LAW_ERR_OK);
                                break;
                        default: 
                                SEL_HALT();
                }

                if(signal == LAW_MODE_SUSPENDED) {
                        task->mode = LAW_MODE_SUSPENDED;
                        continue;
                }
 
                SEL_TEST(law_table_remove(w->table, task->id) == task);
                
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
        law_on_error_t on_error = server->cfg.on_error;
        law_data_t data = server->cfg.data;

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

        law_vers_t version = 0;
        law_id_t id = 0;
        law_event_t event = { .events = LAW_EV_TIM, .data = { .ptr = NULL } };

        now = law_time_millis();

        while(law_timer_peek(timer, &min_expiry, &id, &version)) {

                (void)law_err_clear();

                if(min_expiry > now) break;

                SEL_TEST(law_timer_pop(timer, NULL, NULL, NULL));

                law_task_t *task = law_table_lookup(table, id);
                if(!task) {
                        LAW_ERR_PUSH(LAW_ERR_NOID, "law_table_lookup");
                        (void)on_error(server, LAW_ERR_NOID, data);
                        continue;
                }
                if(task->version != version) {
                        LAW_ERR_PUSH(LAW_ERR_VERS, "law_table_lookup");
                        (void)on_error(server, LAW_ERR_VERS, data);
                        continue;
                }

                (void)law_task_push_event(task, &event);
                (void)law_ready_set_push(ready, task);

                min_expiry = 0;
                id = 0;
                version = 0;
        }

        while(law_evo_next(worker->evo, &event)) {
                
                if(event.data.u64 == 0) continue;
                
                law_slot_t slot;
                law_slot_decode(event.data.u64, &slot);

                law_task_t *task = law_table_lookup(
                        worker->table, 
                        slot.id);
                if(!task) continue;

                event.data.i8 = slot.data;
                
                (void)law_task_push_event(task, &event);
                (void)law_ready_set_push(ready, task);

                event.events = 0;
                event.data.u64 = 0;
        }

        law_msg_t msg;
        memset(&msg, 0, sizeof(law_msg_t));

        event.events = LAW_EV_WAK;
        event.data.ptr = NULL;

        while(law_msg_queue_pop(&worker->messages, &msg) == LAW_ERR_OK) {
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
                
                msg.type = 0;
                msg.data.u64 = 0;
        }

        while(law_table_size(table) <= server->cfg.worker_tasks) {

                law_task_t *task = NULL;
                if(law_task_queue_pop(&worker->incoming, &task) != LAW_ERR_OK) 
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

static void law_worker_run(law_worker_t *w)
{
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
        return s->cfg.on_accept(worker, data.fd, s->cfg.data);
}

static sel_err_t law_server_accept(law_server_t *server) 
{
        while(!law_task_pool_is_empty(server->pool)) {

                law_err_clear();

                const int fd = accept(server->socket, NULL, NULL);

                if(fd == -1) {
                        if(errno == EAGAIN || errno == EWOULDBLOCK) {
                                return LAW_ERR_OK;
                        }
                        LAW_ERR_PUSH(LAW_ERR_SYS, "accept");
                        server->cfg.on_error(
                                server,
                                LAW_ERR_SYS,
                                server->cfg.data);
                        switch(errno) {
                                case EHOSTUNREACH:
                                case ENETUNREACH:
                                case ENONET:
                                case EHOSTDOWN:
                                case ENETDOWN:
                                        return LAW_ERR_OK;
                                case EPROTO:
                                case ENOPROTOOPT:
                                case ECONNABORTED:
                                case EOPNOTSUPP:
                                        continue;
                                default:
                                        return LAW_ERR_SYS;
                        }
                }

                const int flags = fcntl(fd, F_GETFL);
                if(flags == -1) {
                        close(fd);
                        return LAW_ERR_PUSH(SEL_ERR_SYS, "fcntl");
                }

                if(fcntl(fd, F_SETFL, O_NONBLOCK | flags) == -1) {
                        close(fd);
                        return LAW_ERR_PUSH(SEL_ERR_SYS, "fcntl"); 
                }

                law_task_t *const task = law_task_pool_pop(server->pool);
                SEL_TEST(task);

                law_data_t data = { .fd = fd };

                (void)law_task_setup(
                        task, 
                        law_server_genid(server), 
                        law_accept_callback, 
                        data);
                
                law_spawn_dispatch(server, task);
        }

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

        law_msg_t msg;
        memset(&msg, 0, sizeof(law_msg_t));
        msg.type = LAW_MSG_SHUTDOWN;
        msg.data.u64 = 0;

        for(int n = 0; n < s->cfg.workers; ++n) {
                law_msg_queue_push(&s->workers[n]->messages, &msg);
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
        law_err_clear();

        if(s->mode != LAW_MODE_CREATED) 
                return LAW_ERR_MODE;

        s->mode = LAW_MODE_RUNNING;
        
        sel_err_t error = law_server_run_threads(s);
        
        s->mode = LAW_MODE_STOPPED;
        
        return error;
}

sel_err_t law_stop(law_server_t *s)
{
        if(s->mode != LAW_MODE_RUNNING) 
                return LAW_ERR_MODE;

        s->mode = LAW_MODE_STOPPING;
        
        return LAW_ERR_OK;
}
