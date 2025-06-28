#ifndef LAWD_SERVER_H
#define LAWD_SERVER_H

#include "lawd/error.h"
#include "lawd/data.h"
#include "lawd/event.h"
#include "lawd/time.h"
#include "lawd/id.h"
#include <stddef.h>
#include <stdint.h>

/** Network Protocol */
enum law_network_protocol {
        LAW_PROTOCOL_UDP                = 1,            /** UDP over IPv4 */
        LAW_PROTOCOL_UDP6               = 2,            /** UDP over IPv6 */
        LAW_PROTOCOL_TCP                = 3,            /** TCP over IPv4 */
        LAW_PROTOCOL_TCP6               = 4,            /** TCP over IPv6 */
        LAW_PROTOCOL_NONE               = 5             /** No Protocol */
};

/** Network Server */
typedef struct law_server law_server_t;

/** Worker Thread */
typedef struct law_worker law_worker_t;

/** General Callback */
typedef sel_err_t (*law_callback_t)(
        law_worker_t *worker,
        law_data_t data);

/** Accept Callback */
typedef sel_err_t (*law_on_accept_t)(
        law_worker_t *worker, 
        int socket,
        law_data_t data);

/** On Error Callback */
typedef sel_err_t (*law_on_error_t)(
        law_server_t *server,
        sel_err_t error,
        law_data_t data);

/** Server Configuration */
typedef struct law_server_cfg {                            

        int protocol;                           /** Socket Protocol. */
        int port;                               /** Socket Port */
        int backlog;                            /** Socket Listen Backlog */

        int workers;                            /** Number of Worker Threads */
        int worker_tasks;                       /** Max Tasks per Worker */

        int server_timeout;                     /** Server Polling Timeout */
        int worker_timeout;                     /** Worker Polling Timeout */

        size_t stack;                           /** Coroutine Stack Length */
        size_t guards;                          /** Coroutine Stack Guards */
        
        int server_events;                      /** Server Event Buffer */
        int worker_events;                      /** Worker Event Buffer */

        law_on_accept_t on_accept;              /** Accept Callback */
        law_on_error_t on_error;                /** Error Callback */

        law_data_t data;                        /** User Data */

} law_server_cfg_t;

/** 
 * A sane and simple configuration. 
 */
law_server_cfg_t law_server_sanity();

/** 
 * Create a new server and return its pointer.
 * 
 * RETURNS: NULL when out of memory.
 */
law_server_t *law_server_create(law_server_cfg_t *config);

/** 
 * Destroy the server. 
 */
void law_server_destroy(law_server_t *server);

/** 
 * Get the server socket. 
 */
int law_get_server_socket(law_server_t *server);

/** 
 * Get a pointer to the worker's server.
 */
law_server_t *law_get_server(law_worker_t *worker);

/** 
 * Get the worker's active task's id. 
 */
law_id_t law_get_active_id(law_worker_t *worker);

/**
 * Get the worker's id.
 */
int law_get_worker_id(law_worker_t *worker);

/**
 * This function initializes and sets up all the system resources needed for 
 * server operation.  
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS
 */
sel_err_t law_open(law_server_t *server);

/**
 * Close the server, freeing all system resources acquired by law_open.
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS
 */
sel_err_t law_close(law_server_t *server);

/** 
 * Start the server by entering its main loop. 
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_MODE, LAW_ERR_SYS 
 */
sel_err_t law_start(law_server_t *server);

/**
 * Signal the server to (eventually) stop. 
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_MODE
 */
sel_err_t law_stop(law_server_t *server);

/** 
 * Configure events for the file descriptor.  See lawd/events.h for more info.
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS
 */
sel_err_t law_ectl(
        law_worker_t *worker,
        int fd,
        int op,
        law_event_bits_t flags,
        law_event_bits_t events,
        int8_t data);

/** 
 * Wait for events.  Event data is a pointer to the event slot registered 
 * with law_srv_ectl.
 * 
 * RETURNS: 
 *      >= 0 - The number of events received from 0 to max_events.
 *      LAW_ERR_TIMEOUT - Expiration date already in the past. 
 *      LAW_ERR_OOM - Timer ran out of memory. 
 */
int law_ewait(
        law_worker_t *worker,
        law_time_t timeout,
        law_event_t *events,
        int max_events);

/** 
 * Spawn a new task. 
 * 
 * RETURNS: 
 *      LAW_ERR_OK - Task registered successfully.
 *      LAW_ERR_LIMIT - Task limit reached.
 *      LAW_ERR_WANTW - Could not dispatch to any workers.
 */
sel_err_t law_spawn(
        law_server_t *server,
        law_callback_t callback,
        law_data_t data);

/**
 * Lift a non-blocking IO callback on 'fd' to a synchronous call.
 */
sel_err_t law_sync(
        law_worker_t *worker,
        law_time_t timeout,
        sel_err_t (*callback)(int fd, void *state),
        int fd,
        void *state);

#endif
