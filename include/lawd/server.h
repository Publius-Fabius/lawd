#ifndef LAWD_SERVER_H
#define LAWD_SERVER_H

#include "lawd/error.h"
#include "lawd/data.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/** Network Protocol */
enum law_srv_prot {
        LAW_SRV_UDP     = 1,                    /** UDP over IPv4 */
        LAW_SRV_UDP6    = 2,                    /** UDP over IPv6 */
        LAW_SRV_TCP     = 3,                    /** TCP over IPv4 */
        LAW_SRV_TCP6    = 4,                    /** TCP over IPv6 */
        LAW_SRV_NONE    = 5                     /** No Protocol */
};

/** Event Flags */
enum law_srv_flag {
        LAW_SRV_PIN     = 1,                    /** Poll for Input */                       
        LAW_SRV_POUT    = 1 << 1,               /** Poll for Output */
        LAW_SRV_PHUP    = 1 << 2,               /** Poll for a Hangup */
        LAW_SRV_PERR    = 1 << 3                /** Poll for an Error */
};

/** Network Server */
struct law_server;

/** Worker Thread */
struct law_worker;

/** Server Task (Coroutine) */
struct law_task;

/** Server Event */
struct law_event {
        struct law_task *task;
        int socket;
        int flags;
};

/** General Callback */
typedef sel_err_t (*law_srv_call_t)(
        struct law_worker *worker,
        struct law_data *data);

/** Accept Callback */
typedef sel_err_t (*law_srv_accept_t)(
        struct law_worker *worker, 
        int socket,
        struct law_data *data);

/** Server Configuration */
struct law_srv_cfg {                            
        enum law_srv_prot protocol;             /** Socket Protocol. */
        uint16_t port;                          /** Socket Port */
        int backlog;                            /** Accept Backlog */
        int maxconns;                           /** Max Connections */
        int timeout;                            /** Polling timeout */
        size_t stack_length;                    /** Coroutine Stack */
        size_t stack_guard;                     /** Stack Guards */
        uid_t uid;                              /** System User */
        gid_t gid;                              /** System Group */
        int event_buffer;                       /** Event Buffer Size */
        int workers;                            /** Number of Threads */
        law_srv_call_t init;                    /** Worker Init */
        law_srv_call_t tick;                    /** Tick Callback */
        law_srv_accept_t accept;                /** Accept Callback */
        struct law_data data;                   /** User Data */
};

/**
 * A sane and simple configuration.
 */
extern struct law_srv_cfg law_srv_cfg_sanity;

/** 
 * Create a new server. 
 */
struct law_server *law_srv_create(struct law_srv_cfg *config);

/** 
 * Destroy the server. 
 */
void law_srv_destroy(struct law_server *server);

/**
 * Get the server's socket.
 */
int law_srv_socket(struct law_server *server);

/**
 * Start listening for connections and drop root privileges.
 */
sel_err_t law_srv_open(struct law_server *server);

/**
 * Close the server socket and release resources acquired by law_open. 
 * This function is permanent and the server will not be able to be 
 * re-opened without restarting the process.
 */
sel_err_t law_srv_close(struct law_server *server);

/** 
 * Start the server by entering its main loop.
 */
sel_err_t law_srv_start(struct law_server *server);

/**
 * Signal the server to stop.  Eventually the server will return from 
 * law_start with a LAW_ERR_OK code. 
 */
sel_err_t law_srv_stop(struct law_server *server);

/**
 * Get the active task.
 */
struct law_task *law_srv_active(struct law_worker *worker);

/** 
 * Watch the socket.
 */
sel_err_t law_srv_watch(struct law_worker *worker, const int socket);

/** 
 * Unwatch the socket.
 */
sel_err_t law_srv_unwatch(struct law_worker *worker, const int socket);

/**
 * Poll for socket events.
 */
sel_err_t law_srv_poll(struct law_worker *worker, struct law_event *event);

/**
 * Yield the coroutine and wait for IO events to occur or a certain ammount 
 * of time to elapse.
 * LAW_ERR_TTL - The function timed out without an event.
 * LAW_ERR_OK - At least one event was encountered.
 */
sel_err_t law_srv_wait(struct law_worker *worker, int64_t timeout);

/**
 * Spawn a new coroutine.
 */
sel_err_t law_srv_spawn(
        struct law_server *server,
        law_srv_call_t callback,
        struct law_data *data);

#endif
