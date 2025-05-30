#ifndef LAWD_SERVER_H
#define LAWD_SERVER_H

#include "lawd/error.h"
#include "lawd/data.h"
#include "lawd/events.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>

/** Network Protocol */
enum law_srv_prot {
        LAW_SRV_UDP             = 1,            /** UDP over IPv4 */
        LAW_SRV_UDP6            = 2,            /** UDP over IPv6 */
        LAW_SRV_TCP             = 3,            /** TCP over IPv4 */
        LAW_SRV_TCP6            = 4,            /** TCP over IPv6 */
        LAW_SRV_NONE            = 5             /** No Protocol */
};

/** Network Server */
struct law_server;

/** Worker Thread */
struct law_worker;

/** Coroutine Task */
struct law_task;

/** Generalized Callback */
typedef sel_err_t (*law_srv_call_t)(
        struct law_worker *worker,
        struct law_data data);

/** Socket Accept Callback */
typedef sel_err_t (*law_srv_accept_t)(
        struct law_worker *worker, 
        int socket,
        struct law_data data);

/** Server Configuration */
struct law_srv_cfg {                            
        enum law_srv_prot protocol;             /** Socket Protocol. */
        uint16_t port;                          /** Socket Port */
        int backlog;                            /** Accept Backlog */
        int maxconns;                           /** Max Connections */
        int server_timeout;                     /** Server Polling Timeout */
        int worker_timeout;                     /** Worker Polling Timeout */
        size_t stack_length;                    /** Coroutine Stack */
        size_t stack_guard;                     /** Stack Guards */
        uid_t uid;                              /** System User */
        gid_t gid;                              /** System Group */
        int server_events;                      /** Server Event Buffer */
        int worker_events;                      /** Worker Event Buffer */
        int workers;                            /** Number of Threads */
        law_srv_call_t init;                    /** Worker Init */
        law_srv_call_t tick;                    /** Tick Callback */
        law_srv_accept_t accept;                /** Accept Callback */
        struct law_data data;                   /** User Data */
        FILE *errors;                           /** Error Log */
};

struct law_slot {
        int fd;
        struct law_task *task;
        struct law_data data;
};

/** A sane and simple configuration. */
struct law_srv_cfg law_srv_sanity();

/** Create a new server. */
struct law_server *law_srv_create(struct law_srv_cfg *config);

/** Destroy the server. */
void law_srv_destroy(struct law_server *server);

/** Get the server's socket. */
int law_srv_socket(struct law_server *server);

/**  Get the server's error log. */
FILE *law_srv_errors(struct law_server *server);

/** Start listening for connections and drop root privileges. */
sel_err_t law_srv_open(struct law_server *server);

/**
 * Close the server socket and release resources acquired by law_open. 
 * This function is permanent and the server will not be able to be 
 * re-opened without restarting the process.
 */
sel_err_t law_srv_close(struct law_server *server);

/** Start the server by entering its main loop. */
sel_err_t law_srv_start(struct law_server *server);

/**
 * Signal the server to stop.  Eventually, the server will return from 
 * law_start with a LAW_ERR_OK code. 
 */
sel_err_t law_srv_stop(struct law_server *server);

/** Get the worker's server. */
struct law_server *law_srv_server(struct law_worker *worker);

/** Get the worker's active task. */
struct law_task *law_srv_active(struct law_worker *worker);

/** 
 * Configure events for the file descriptor. 
 * 
 * RETURNS: LAW_ERR_OK (0) on success 
 * 
 * ERRORS: LAW_ERR_SYS 
 */
sel_err_t law_srv_ectl(
        struct law_worker *worker,
        struct law_slot *slot,
        const int op,
        const uint32_t flags,
        const uint32_t slots);

/** 
 * Wait for events. 
 * 
 * RETURNS: The number of events received from 0 to max_events.
 * 
 * ERRORS:
 *      LAW_ERR_TTL - Expiration already reached. 
 *      LAW_ER_OOM - Timer queue out of memory. 
 */
int law_srv_ewait(
        struct law_worker *worker,
        const int64_t expiry,
        struct law_ev *events,
        const int max_events);

/** 
 * Add the task to the notification queue.  This function returns immediately.
 * 
 * RETURNS: LAW_ERR_OK (0) on success.
 * 
 * ERRORS:
 *      LAW_ERR_WNTW - Unable to enqueue the worker thread (pipe full);
 */
sel_err_t law_srv_notify(struct law_task *task);

/** 
 * Spawn a new task.  This function returns immediately.
 * 
 * RETURNS: LAW_ERR_OK (0) on success.
 * 
 * ERRORS: 
 *      LAW_ERR_OOM - Ran out of memory creating a new task.
 *      LAW_ERR_WNTW - Unable to enqueue the worker's channel (pipe full).
 *      LAW_ERR_OK - All OK
 */
sel_err_t law_srv_spawn(
        struct law_server *server,
        law_srv_call_t callback,
        struct law_data data);

#endif
