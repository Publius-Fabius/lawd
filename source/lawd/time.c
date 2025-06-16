
#define _POSIX_C_SOURCE 200809L

#include <time.h>
#include <stdlib.h>
#include "lawd/time.h"
#include "pubmt/binary_heap.h"

char *law_time_datetime(struct law_time_dt_buf *buf)
{
        /* 2007-04-05T14:30Z */
        static const char *FORMAT = "%Y-%m-%dT%H:%M:%SZ";
        struct tm tm;
        time_t t;
        time(&t);
        gmtime_r(&t, &tm);
        strftime(buf->bytes, LAW_TIME_DT_BUF_LEN, FORMAT, &tm);
        return buf->bytes;
}

int64_t law_time_millis()
{
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        return (int64_t)(ts.tv_sec) * 1000 + (int64_t)(ts.tv_nsec) / 1000000;
}

int law_time_sleep(law_time_t millis)
{
        struct timespec spec = { 
                .tv_sec = 0,
                .tv_nsec = 1000000 };
        return nanosleep(&spec, NULL);
}

typedef struct law_timer_elem {

        law_time_t expiration;

        law_id_t identifier;

        uint32_t version;

} law_timer_elem_t;

struct law_timer {

        size_t size, capacity;

        law_timer_elem_t *buffer;
};

void *law_timer_alloc(const size_t size, void *state)
{
        return malloc(size);
}

void law_timer_free(void *pointer, void *state)
{
        free(pointer);
}

void *law_timer_realloc(void *pointer, const size_t size, void *state)
{
        return realloc(pointer, size);
}

pmt_da_alloc_t law_timer_get_alloc(void *array)
{
        return law_timer_alloc;
}

pmt_da_realloc_t law_timer_get_realloc(void *array)
{
        return law_timer_realloc;
}

pmt_da_free_t law_timer_get_free(void *array)
{
        return law_timer_free;
}

void *law_timer_get_alloc_state(void *array)
{
        return NULL;
}

size_t law_timer_get_element_size(void *array)
{
        return sizeof(law_timer_elem_t);
}

size_t law_timer_get_capacity(void *timer)
{
        return ((law_timer_t*)timer)->capacity;
}

void law_timer_set_capacity(void *timer, const size_t capacity)
{
        ((law_timer_t*)timer)->capacity = capacity;
}

size_t law_timer_get_size(void *timer)
{
        return ((law_timer_t*)timer)->size;
}

void law_timer_set_size(void *timer, const size_t size)
{
        ((law_timer_t*)timer)->size = size;
}

void *law_timer_get_buffer(void *timer)
{
        return ((law_timer_t*)timer)->buffer;
}

void law_timer_set_buffer(void *timer, void *buffer)
{
        ((law_timer_t*)timer)->buffer = buffer;
}

void law_timer_swap(void *elem_a, void *elem_b)
{
        law_timer_elem_t *a = elem_a, *b = elem_b, tmp;
        tmp = *a;
        *a = *b;
        *b = tmp;
}

pmt_bh_swap_t law_timer_get_swap(void *heap)
{
        return law_timer_swap;
}

bool law_timer_less_than(void *elem_a, void *elem_b) 
{
        law_timer_elem_t *a = elem_a, *b = elem_b;
        return a->expiration < b->expiration;
}

pmt_bh_less_than_t law_timer_get_less_than(void *heap)
{
        return law_timer_less_than;
}

pmt_bh_iface_t law_timer_iface = {
        .array_iface = {
                .get_alloc = law_timer_get_alloc,
                .get_realloc = law_timer_get_realloc,
                .get_free = law_timer_get_free,
                .get_alloc_state = law_timer_get_alloc_state,
                .get_element_size = law_timer_get_element_size,
                .get_capacity = law_timer_get_capacity,
                .set_capacity = law_timer_set_capacity,
                .get_size = law_timer_get_size,
                .set_size = law_timer_set_size,
                .get_buffer = law_timer_get_buffer,
                .set_buffer =  law_timer_set_buffer
        },
        .get_swap = law_timer_get_swap,
        .get_less_than = law_timer_get_less_than
};

law_timer_t *law_timer_create(size_t capacity)
{
        law_timer_t *timer = calloc(1, sizeof(law_timer_t));
        if(!timer) return NULL;

        if(!pmt_da_create(&law_timer_iface.array_iface, timer, capacity)) {
                free(timer);
                return NULL;
        }

        return timer;
}

void law_timer_destroy(law_timer_t *timer)
{
        if(!timer) return;
        pmt_da_destroy(&law_timer_iface.array_iface, timer);
        free(timer);
}

bool law_timer_insert(
        law_timer_t *timer, 
        law_time_t expiration,
        law_id_t identifier,
        uint32_t version)
{
        law_timer_elem_t elem = {
                .expiration = expiration,
                .identifier = identifier,
                .version = version};

        return pmt_bh_insert(&law_timer_iface, timer, &elem) ? true : false;
}

bool law_timer_peek( 
        law_timer_t *timer,
        law_time_t *expiration,
        law_id_t *identifier,
        uint32_t *version)
{
        law_timer_elem_t *elem = pmt_bh_peek(&law_timer_iface, timer);

        if(!elem) {
                return false;
        }

        if(expiration) *expiration = elem->expiration;
        if(identifier) *identifier = elem->identifier;
        if(version) *version = elem->version;

        return true;
}

bool law_timer_pop( 
        law_timer_t *timer,
        law_time_t *expiration,
        law_id_t *identifier,
        uint32_t *version)
{
        law_timer_elem_t elem;

        if(!pmt_bh_pop(&law_timer_iface, timer, &elem)) {
                return false;
        }

        if(expiration) *expiration = elem.expiration;
        if(identifier) *identifier = elem.identifier;
        if(version) *version = elem.version;

        return true;
}

size_t law_timer_size(law_timer_t *timer)
{
        return timer->size;
}