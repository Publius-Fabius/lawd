
#include "lawd/events.h"
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#ifdef __linux__

struct law_evo {
        int fd, nevents, offset, max_events;
        struct epoll_event *events;
};

law_evo_t *law_evo_create(int max_events)
{
        assert(max_events > 0);

        struct law_evo *evo = malloc(sizeof(struct law_evo));
        if(!evo) return NULL;

        evo->nevents = 0;
        evo->offset = 0;
        evo->max_events = max_events;
        evo->fd = -1;

        evo->events = malloc(
                (uint32_t)max_events * sizeof(struct epoll_event));
        if(!evo->events) {
                free(evo);
                return NULL;
        }

        return evo;
}

void law_evo_destroy(struct law_evo *evo)
{
        assert(evo);
        free(evo->events);
        free(evo);
}

int law_evo_open(law_evo_t *evo)
{
        evo->fd = epoll_create(evo->max_events); 
        return evo->fd == -1 ? -1 : 0;
}

int law_evo_close(law_evo_t *evo)
{
        return close(evo->fd) == -1 ? -1 : 0;
}

int law_evo_ctl(
        struct law_evo *evo, 
        int fd,
        int op, 
        law_event_bits_t flags, 
        struct law_event *event)
{
        struct epoll_event ep_ev = {
                .events = flags | event->events,
                .data.u64 = event->data.u64 };

        if(epoll_ctl(evo->fd, op, fd, &ep_ev) == -1)
                return -1;

        return 0;
}

int law_evo_wait(struct law_evo *evo, int timeout)
{
        evo->nevents = evo->offset = 0;
        return epoll_wait(
                evo->fd, evo->events, evo->max_events, timeout);
}

bool law_evo_next(struct law_evo *evo, struct law_event *event)
{
        assert(evo && evo->offset <= evo->nevents);
        if(evo->offset == evo->nevents) 
                return false;
        struct epoll_event *ep_ev = evo->events + evo->offset++;
        if(event) {
                event->events = ep_ev->events;
                event->data.u64 = ep_ev->data.u64;
        }
        return true;
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

