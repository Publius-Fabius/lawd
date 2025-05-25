
#include "lawd/priority.h"
#include "lawd/data.h"
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <errno.h>

struct law_pq_node {
        struct law_data priority;
        uint32_t version;
        void *value;
};

struct law_pq {
        struct law_pq_node *nodes;
        size_t size;
        size_t capacity;
        law_pq_cmp_t compare;
};

size_t law_pq_capacity(struct law_pq *queue)
{
        return queue->capacity;
}

size_t law_pq_size(struct law_pq *queue)
{
        return queue->size;
}

struct law_pq *law_pq_create(law_pq_cmp_t compare)
{
        static const size_t INIT_CAP = 8;
        struct law_pq *queue = malloc(sizeof(struct law_pq));
        if(!queue) return NULL;
        queue->compare = compare;
        queue->capacity = INIT_CAP;
        queue->size = 0;
        queue->nodes = malloc(INIT_CAP * sizeof(struct law_pq_node));
        if(!queue->nodes) {
                free(queue);
                return NULL;
        }
        return queue;
}

void law_pq_destroy(struct law_pq *queue)
{
        if(!queue) return;
        free(queue->nodes);
        free(queue);
}

static struct law_pq *law_pq_expand(struct law_pq *queue)
{
        const size_t new_cap = queue->capacity * 2;
        struct law_pq_node *nodes = queue->nodes;
        nodes = realloc(nodes, new_cap * sizeof(struct law_pq_node));
        if(!nodes) return NULL;
        queue->nodes = nodes;
        queue->capacity = new_cap;
        return queue;
}

static void law_pq_swap(struct law_pq_node *a, struct law_pq_node *b) 
{
        struct law_pq_node tmp = *a;
        *a = *b;
        *b = tmp;
}

static void law_pq_heapify_up(
        law_pq_cmp_t cmp,
        struct law_pq_node *ns, 
        ssize_t index) 
{
        while(index > 0) {
                ssize_t parent = (index - 1) / 2;
                if(cmp(ns[index].priority, ns[parent].priority)) {
                        law_pq_swap(ns + index, ns + parent);
                        index = parent;
                } else {
                        break;
                }
        }
}

int law_pq_insert(
        struct law_pq *q,
        struct law_data priority,
        const uint32_t version,
        void *value)
{
        assert(q && q->size <= q->capacity);
        if(q->size == q->capacity) {
                if(!law_pq_expand(q)) {
                        errno = ENOMEM;
                        return -1;
                }
        }
        struct law_pq_node *node = q->nodes + q->size;
        node->priority = priority;
        node->version = version;
        node->value = value;
        law_pq_heapify_up(q->compare, q->nodes, (ssize_t)q->size++);
        return 1;
}

static void law_pq_heapify_down(
        law_pq_cmp_t cmp,
        struct law_pq_node *ns,
        const size_t size, 
        size_t index) 
{
        for(;;) {
                size_t l = 2 * index + 1;
                size_t r = 2 * index + 2;
                size_t min = index;

                if(l < size) {
                        if(cmp(ns[l].priority, ns[min].priority)) {
                                min = l;
                                goto BRANCH_A;
                        }
                } 
                if(r < size) {
                        if(cmp(ns[r].priority, ns[min].priority)) {
                                min = r;
                                goto BRANCH_B;
                        } 
                }
                
                break;

                BRANCH_A:
                if(r < size) {
                        if(cmp(ns[r].priority, ns[min].priority)) {
                                min = r;
                        }
                }

                BRANCH_B:
                law_pq_swap(ns + index, ns + min);
                index = min;
        }
}

int law_pq_pop(
        struct law_pq *q,
        struct law_data *priority,
        uint32_t *version,
        void **value)
{
        assert(q);
        if(q->size == 0) return 0;
        struct law_pq_node *ns = q->nodes;
        if(priority) *priority = ns->priority;
        if(version) *version = ns->version;
        if(value) *value = ns->value;
        law_pq_swap(ns, ns + --q->size);
        law_pq_heapify_down(q->compare, ns, q->size, 0);
        return 1;
}

int law_pq_peek(
        struct law_pq *queue,
        struct law_data *priority,
        uint32_t *version,
        void **value)
{
        assert(queue);
        if(queue->size == 0) return 0;
        struct law_pq_node *ns = queue->nodes;
        if(priority) *priority = ns->priority;
        if(version) *version = ns->version;
        if(value) *value = ns->value;
        return 1;
}
