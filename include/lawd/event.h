/** Shallow wrapper generalizing epoll/kqueue. */

#ifndef LAWD_EVENT_H
#define LAWD_EVENT_H

#include "lawd/data.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __linux__

#include <sys/epoll.h>

/** Event Operation */
enum law_ev_op {
        LAW_EV_ADD              = EPOLL_CTL_ADD,
        LAW_EV_MOD              = EPOLL_CTL_MOD,
        LAW_EV_DEL              = EPOLL_CTL_DEL
}; 

/** Event Flag */
enum law_ev_flag {
        LAW_EV_ONESHOT          = EPOLLONESHOT,
        LAW_EV_EDGE             = EPOLLET
};

/** Event Type */
enum law_ev_type {
        LAW_EV_R                = EPOLLIN,
        LAW_EV_W                = EPOLLOUT,
        LAW_EV_ERR              = EPOLLERR,
        LAW_EV_HUP              = EPOLLHUP,
        LAW_EV_TIM              = 1u << 19,
        LAW_EV_MSG              = 1u << 20,
        LAW_EV_WAK              = 1u << 18
};

#elif   defined(__APPLE__) || \
        defined(__FreeBSD__) || \
        defined(__NetBSD__) || \
        defined(__OpenBSD__) || \
        defined(__DragonFly__)

#error "KQUEUE SYSTEMS NOT YET SUPPORTED"

#else

#error "SYSTEM NOT SUPPORTED"

#endif

/** Event Bits */
typedef uint32_t law_event_bits_t;

/** IO Event */
typedef struct law_event {

        law_event_bits_t events;

        struct law_data data;
        
} law_event_t;

/** IO Event Object */
typedef struct law_evo law_evo_t;

/** 
 * Create a new event object. 
 * 
 * RETURNS:
 *      LAW_ERR_OK - Success 
 *      LAW_ERR_OOM - Ran out of memory 
 *      LAW_ERR_SYS - System call error.
 */
law_evo_t *law_evo_create(int max_events);

/** 
 * Destroy the event object. 
 */
void law_evo_destroy(law_evo_t *evo);

/**
 * Open evo. 
 * 
 * RETURNS: -1 error, 0 success
 */
int law_evo_open(law_evo_t *evo);

/**
 * Close evo. 
 * 
 * RETURNS: -1 error, 0 success
 */
int law_evo_close(law_evo_t *evo);

/** 
 * Control file descriptor events for an event object. 
 * 
 * RETURNS: -1 error, 0 success.
 */
int law_evo_ctl(
        law_evo_t *evo, 
        int fd,
        int op, 
        law_event_bits_t flags, 
        law_event_t *event);

/** 
 * Wait timeout milliseconds for events. 
 * 
 * RETURNS: 
 *      -1 - Error
 *      0 to positive N - The number of events ready for inspection.
 */
int law_evo_wait(law_evo_t *evo, int timeout);

/** 
 * Iterate through the event object. 
 * 
 * RETURNS: A value of 'false' means the iteration is finished.
 */
bool law_evo_next(law_evo_t *evo, law_event_t *event);

#endif