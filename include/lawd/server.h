#ifndef LAWD_SERVER_H
#define LAWD_SERVER_H

#include "lawd/error.h"
#include <stddef.h>
#include <poll.h>
#include <stdint.h>
#include <sys/types.h>

/** Network Protocol */
enum law_srv_prot {
        LAW_SRV_UDP     = 1,                    /** UDP over IPv4 */
        LAW_SRV_UDP6    = 2,                    /** UDP over IPv6 */
        LAW_SRV_TCP     = 3,                    /** TCP over IPv4 */
        LAW_SRV_TCP6    = 4                     /** TCP over IPv6 */
};

/** Non-Blocking POSIX Network Server */
struct law_srv;                                 

/** Accept Hook */
typedef sel_err_t (*law_accept_t)(
        struct law_srv *server, 
        int client_socket,
        void *user_state);

/** Loop Hook */
typedef sel_err_t (*law_loop_t)(
        struct law_srv* server,
        void *user_state);

/** Server Configuration */
struct law_srv_cfg {                            
        enum law_srv_prot protocol;             /** Socket Protocol. */
        uint16_t port;                          /** Socket Port */
        int backlog;                            /** Accept Backlog */
        int maxconns;                           /** Max Connections */
        int timeout;                            /** Polling timeout */
        size_t stacklen;                        /** Coroutine Stack */
        size_t guardlen;                        /** Stack Guards */
        uid_t uid;                              /** System User */
        gid_t gid;                              /** System Group */
        law_accept_t accept;                    /** Accept Callback */
        law_loop_t loop;                        /** Loop Callback */
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
 * Acquire polling descriptor for one loop.
 * @param server The server.
 * @return A polling desriptor.
 */
struct pollfd *law_srv_lease(struct law_srv *server);

/**
 * Poll events.
 */
int law_srv_poll(
        struct law_srv *server, 
        const int socket,
        const int events);

#endif
