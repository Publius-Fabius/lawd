/** Shallow wrapper generalizing epoll/kqueue. */

#ifndef LAWD_EVENTS_H
#define LAWD_EVENTS_H

#include "lawd/data.h"
#include <stdint.h>

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
        LAW_EV_TTL              = 1u << 19,
        LAW_EV_INT              = 1u << 20
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

/** IO Event */
struct law_ev {
        uint32_t events;
        struct law_data data;
};

/** IO Event Object */
struct law_evo;

/** Create a new event object. */
struct law_evo *law_evo_create(const int max_events);

/** Destroy the event object. */
void law_evo_destroy(struct law_evo *evo);

/** Control file descriptor events for an event object. */
int law_evo_ctl(
        struct law_evo *evo, 
        const int fd,
        const int op, 
        const uint32_t flags, 
        struct law_ev *event);

/** Wait timeout milliseconds for events. */
int law_evo_wait(struct law_evo *evo, const int timeout);

/** Iterate through the event object. */
struct law_evo *law_evo_next(struct law_evo *evo, struct law_ev *event);

#endif