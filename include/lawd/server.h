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

/** Generalized Callback */
typedef sel_err_t (*law_callback_t)(law_worker_t *worker, law_data_t data);

/** Socket Accept Callback */
typedef sel_err_t (*law_accept_t)(
        law_worker_t *worker, 
        int socket,
        law_data_t data);

/** Server Configuration */
typedef struct law_server_config {                            

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

        law_callback_t init;                    /** Worker Init */
        law_callback_t tick;                    /** Tick Callback */
        law_accept_t accept;                    /** Accept Callback */
        law_data_t data;                        /** User Data */

} law_server_config_t;

/** 
 * A sane and simple configuration. 
 */
law_server_config_t law_server_sanity();

/** 
 * Create a new server and return its pointer.
 * 
 * RETURNS: NULL when out of memory.
 */
law_server_t *law_server_create(law_server_config_t *config);

/** 
 * Destroy the server. 
 */
void law_server_destroy(law_server_t *server);

/** 
 * Get the server socket. 
 */
int law_get_socket(law_server_t *server);

/** 
 * Get a pointer to the worker's server.
 */
law_server_t *law_get_server(law_worker_t *worker);

/** 
 * Get the worker's active task. 
 */
law_id_t law_get_active(law_worker_t *worker);

/** 
 * Create the server socket and set it to non-blocking mode.
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS (check errno)
 */
sel_err_t law_create_socket(law_server_t *server, int *fd);

/** 
 * Bind the server socket to its port.
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS (check errno)
 */
sel_err_t law_bind_socket(law_server_t *server);

/** 
 * Start listening on the server socket. 
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS (check errno)
 */
sel_err_t law_listen(law_server_t *server);

/**
 * This function initializes and sets up all the system resources needed for 
 * server operation. 
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS (check errno)
 */
sel_err_t law_open(law_server_t *server);

/**
 * Close the server, freeing all system resources acquired by law_open.
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_SYS (check errno)
 */
sel_err_t law_close(law_server_t *server);

/** 
 * Start the server by entering its main loop. 
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_MODE, LAW_ERR_SYS (check errno)
 */
sel_err_t law_start(law_server_t *server);

/**
 * Signal the server to (eventually) stop. 
 * 
 * RETURNS: LAW_ERR_OK, LAW_ERR_MODE
 */
sel_err_t law_stop(law_server_t *server);

/** 
 * Set slot index to file descriptor.  Valid slot indices are 0 to 15.
 * 
 * RETURNS: LAW_ERR_OOB, LAW_ERR_OK
 */
sel_err_t law_set_slot(law_worker_t *worker, int index, int fd);

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
 *      LAW_ERR_TTL - Expiration date already in the past. 
 *      LAW_ER_OOM - Timer queue out of memory. 
 */
int law_ewait(
        law_worker_t *worker,
        law_time_t expiration_date,
        law_event_t *events,
        int max_events);

/** 
 * Spawn a new task.  Optionally retrieving the task's worker and id.
 * 
 * RETURNS: 
 *      LAW_ERR_OK - Task registered successfully.
 *      LAW_ERR_LIM - Task limit reached.
 *      LAW_ERR_WNTW - Could not dispatch to any workers.
 */
sel_err_t law_spawn(
        law_server_t *server,
        law_callback_t callback,
        law_data_t data);

#endif
