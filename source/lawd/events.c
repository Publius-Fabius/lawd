#include "lawd/events.h"
#include <assert.h>
#include <stdlib.h>

#ifdef __linux__

struct law_evo {
        int fd, nevents, offset, max_events;
        struct epoll_event *events;
};

struct law_evo *law_evo_create(const int max_events)
{
        assert(max_events > 0);
        struct law_evo *evo = malloc(sizeof(struct law_evo));
        if(!evo) return NULL;
        evo->nevents = evo->offset = 0;
        evo->max_events = max_events;
        evo->events = malloc(max_events * sizeof(struct epoll_event));
        if(!evo->events) {
                free(evo);
                return NULL;
        }
        evo->fd = epoll_create(16); 
        if(!evo->fd) {
                free(evo->events);
                free(evo);
                return NULL;
        }
        return evo;
}

void law_evo_destroy(struct law_evo *evo)
{
        assert(evo);
        close(evo->fd);
        free(evo->events);
        free(evo);
}

int law_evo_ctl(
        struct law_evo *evo, 
        const int fd,
        const enum law_ev_op op, 
        const enum law_ev_flag flags, 
        struct law_ev *event)
{
        struct epoll_event ep_ev = {
                .events = flags | event->events,
                .data.u64 = event->data.u64 };
        return epoll_ctl(evo->fd, op, fd, &ep_ev);
}

int law_evo_wait(struct law_evo *evo, const int timeout)
{
        evo->nevents = evo->offset = 0;
        const int nevents = epoll_wait(
                evo->fd, evo->events, evo->max_events, timeout);
        if(nevents < 0) {
                return -1;
        } 
        evo->nevents = nevents;
        return nevents;
}

int law_evo_next(struct law_evo *evo, struct law_ev *event)
{
        assert(evo && evo->offset <= evo->nevents);
        if(evo->offset == evo->nevents) return 0;
        struct epoll_event *ep_ev = evo->events + evo->offset++;
        if(event) {
                event->events = ep_ev->events;
                event->data.u64 = ep_ev->data.u64;
        }
        return 1;
}

#elif   defined(__APPLE__) || \
        defined(__FreeBSD__) || \
        defined(__NetBSD__) || \
        defined(__OpenBSD__) || \
        defined(__DragonFly__)
#error "KQUEUE SYSTEMS NOT YET SUPPORTED"
#else
#error "SYSTEM NOT SUPPORTED"
#endif

