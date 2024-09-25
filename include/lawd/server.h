#ifndef LAWD_SERVER_H
#define LAWD_SERVER_H

#include "lawd/error.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>

/** Network Protocol */
enum law_srv_prot {
        LAW_SRV_UDP     = 1,                    /** UDP over IPv4 */
        LAW_SRV_UDP6    = 2,                    /** UDP over IPv6 */
        LAW_SRV_TCP     = 3,                    /** TCP over IPv4 */
        LAW_SRV_TCP6    = 4,                    /** TCP over IPv6 */
        LAW_SRV_NONE    = 5                     /** No Protocol */
};

/** POSIX Network Server */
struct law_srv;

/** Initialize Worker Thread */
typedef sel_err_t (*law_srv_init_t)(
        struct law_srv *server,
        void *state);

/** Server Coroutine Callback */
typedef sel_err_t (*law_srv_call_t)(
        struct law_srv *server, 
        int socket,
        void *state);

/** Server Loop Callback */
typedef sel_err_t (*law_srv_tick_t)(
        struct law_srv *server,
        void *state);

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
        int workers;                            /** Number of Threads */
        law_srv_init_t init;                    /** Initialize Worker */
        law_srv_call_t accept;                  /** Accept Callback */
        law_srv_tick_t tick;                    /** Tick Callback */
        void *state;                            /** User State */
};

/** 
 * Create a new server. 
 * @param config The server's configuration.
 * @return A newly allocated server.
 */
struct law_srv *law_srv_create(struct law_srv_cfg *config);

/** 
 * Destroy the server. 
 * @param server The server to destroy.
 */
void law_srv_destroy(struct law_srv *server);

/**
 * Get the server's socket.
 * @param server The server.
 * @return The server socket.
 */
int law_srv_socket(struct law_srv *server);

/**
 * Start listening for connections and drop root privileges.
 * @param server The server to open.
 * @return A status code.
 */
sel_err_t law_srv_open(struct law_srv *server);

/**
 * Close the server socket and release resources acquired by law_open. 
 * This function is permanent and the server will not be able to be 
 * re-opened without restarting the process.
 * @param server The server to close.
 * @return A status code.
 */
sel_err_t law_srv_close(struct law_srv *server);

/** 
 * Start the server by entering its main loop.
 * @param server The server to start.
 * @return A status code.
 */
sel_err_t law_srv_start(struct law_srv *server);

/**
 * Signal the server to stop.  Eventually the server will return from 
 * law_start with a LAW_ERR_OK code. 
 * @param server The server to suspend.
 * @return A status code.
 */
sel_err_t law_srv_stop(struct law_srv *server);

/** 
 * Yield the current coroutine's execution environment back to the 
 * scheduler.
 * @param server The server.
 * @return A status code.
 */
sel_err_t law_srv_yield(struct law_srv *server);

/**
 * Poll for socket events.
 */
sel_err_t law_srv_poll(
        struct law_srv *server,
        const int socket,
        const int events,
        int *revents);

/**
 * Wait for IO events to occur or a certain ammount of time to elapse.
 * @param server The server.
 * @param timeout How long to wait.
 * @returns 
 * LAW_ERR_TTL - The function timed out without an event.
 * LAW_ERR_OK - At least one event was encountered.
 */
sel_err_t law_srv_wait(struct law_srv *server, int64_t timeout);

/**
 * Spawn a new coroutine.
 * @param server The server
 * @param callback The callback to run within a new coroutine.
 * @param socket The socket to attach to this coroutine.
 * @param state The user defined state for the associated callback.
 * @return An error code.
 */
sel_err_t law_srv_spawn(
        struct law_srv *server,
        law_srv_call_t callback,
        const int socket,
        void *state);

#endif
