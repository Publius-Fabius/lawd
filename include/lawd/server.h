#ifndef LAWD_SERVER_H
#define LAWD_SERVER_H

#include "lawd/error.h"
#include "lawd/data.h"
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

/** Event Options */
enum law_srv_evopt {
        LAW_SRV_POLLIN          = EPOLLIN,      /** Poll for Input */                       
        LAW_SRV_POLLOUT         = EPOLLOUT,     /** Poll for Output */
        LAW_SRV_POLLHUP         = EPOLLHUP,     /** Poll for a Hangup */
        LAW_SRV_POLLERR         = EPOLLERR      /** Poll for an Error */
};

/** Server Signal */
enum law_srv_sig {
        LAW_SRV_SIGIO           = 1,            /** IO Signal */
        LAW_SRV_SIGINT          = 2,            /** Notification Signal */
        LAW_SRV_SIGTTL          = 3             /** Timeout Signal */
};

/** Network Server */
struct law_server;

/** Worker Thread */
struct law_worker;

/** Worker Task */
struct law_task;

/** IO Event */
struct law_event {
        unsigned int events;            /** Events to scan for. */
        unsigned int revents;           /** Returned events. */
        struct law_task *task;          /** The task the event belongs to. */
};

/** General Callback */
typedef sel_err_t (*law_srv_call_t)(
        struct law_worker *worker,
        struct law_data data);

/** Accept Callback */
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
        int worker_timeout;                     /** Worker Polling timeout */
        int server_timeout;                     /** Server Polling Timeout */
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
        FILE *errors;                           /** Error Log */
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

/** Add a file descriptor to the polling set. */
sel_err_t law_srv_add(
        struct law_worker *worker, 
        const int fd,
        struct law_event *event);

/** Modify the events the file descriptor listens for. */
sel_err_t law_srv_mod(
        struct law_worker *worker, 
        const int fd,
        struct law_event *event);

/** Remove a file descriptor from the polling set. */
sel_err_t law_srv_del(struct law_worker *worker, const int fd);

/** 
 * Yield the current coroutine for timeout milliseconds or until a 
 * notification is received. 
 */
sel_err_t law_srv_sleep(struct law_worker *worker, const int64_t timeout);

/** Add the task to the notification queue. */
sel_err_t law_srv_notify(struct law_task *task);

/** Spawn a new coroutine. */
sel_err_t law_srv_spawn(
        struct law_server *server,
        law_srv_call_t callback,
        struct law_data *data);

/** IO Callback - One Argument */
typedef sel_err_t (*law_srv_io1_t)(void *arg);

/** IO Callback - Two Arguments */
typedef sel_err_t (*law_srv_io2_t)(void *arg0, void *arg1);

/** Wait for an IO action to succeed. */
sel_err_t law_srv_await1(
        struct law_worker *worker,
        const int fd,
        const int64_t timeout,
        law_srv_io1_t callback,
        void *arg);

/** Wait for an IO action to succeed. */
sel_err_t law_srv_await2(
        struct law_worker *worker,
        const int fd,
        const int64_t timeout,
        law_srv_io2_t callback,
        void *arg0,
        void *arg1);

#endif
