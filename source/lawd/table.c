
#include "lawd/table.h"

/** Hash Table Node */
typedef struct law_table_node {
        
        law_id_t id; 

        void *pointer;

        void *next;

} law_table_node_t;

struct law_table {

    size_t capacity, size;

    void **buffer;
};

static void *law_table_get_buffer(void *map)
{
        return ((law_table_t*)map)->buffer;
}

static void law_table_set_buffer(void *map, void *buffer)
{
        ((law_table_t*)map)->buffer = buffer;
}

static size_t law_table_get_size(void *map)
{
        return ((law_table_t*)map)->size;
}

static void law_table_set_size(void *map, const size_t size)
{
        ((law_table_t*)map)->size = size;
}

static size_t law_table_get_capacity(void *map)
{
        return ((law_table_t*)map)->capacity;
}

static void law_table_set_capacity(void *map, const size_t capacity)
{
        ((law_table_t*)map)->capacity = capacity;
}

static size_t law_table_get_element_size(void *map)
{
        return sizeof(struct law_table_node);
}

static void *law_table_alloc(const size_t nbytes, void *alloc_state)
{
        return malloc(nbytes);
}

static void *law_table_realloc(
        void *pointer, 
        const size_t nbytes, 
        void *alloc_state)
{
        return realloc(pointer, nbytes);
}

static void law_table_free(void *pointer, void *alloc_state)
{
        free(pointer);
}

static pmt_da_alloc_t law_table_get_alloc(void *map)
{
        return law_table_alloc;
}

static pmt_da_realloc_t law_table_get_realloc(void *map)
{
        return law_table_realloc;
}

static pmt_da_free_t law_table_get_free(void *map)
{
        return law_table_free;
}

static void *law_table_get_alloc_state(void *map)
{
        return NULL;
}

static bool law_table_equals(void *key_a, void *key_b)
{
        return *((int*)key_a) == *((int*)key_b);
}

static size_t law_table_hash(void *ptr)
{
        return pmt_hm_fnv(ptr, sizeof(int));
}

static pmt_hm_equals_t law_table_get_equals(void *map)
{
        return law_table_equals;
}

static pmt_hm_hash_t law_table_get_hash(void *map)
{
        return law_table_hash;
}

void *law_table_node_get_next(void *node)
{
        return ((law_table_node_t*)node)->next;
}

void law_table_node_set_next(void *node)
{
        ((law_table_node_t*)node)->next = node;
}

void *law_table_node_get_key(void *node)
{
        return &((law_table_node_t*)node)->id;
}

pmt_hm_iface_t law_table_iface = {
        .array_iface = {
                .get_alloc = law_table_get_alloc,
                .get_realloc = law_table_get_realloc,
                .get_alloc_state = law_table_get_alloc_state,
                .get_free = law_table_get_free,
                .get_buffer = law_table_get_buffer,
                .set_buffer = law_table_set_buffer,
                .get_capacity = law_table_get_capacity,
                .set_capacity = law_table_set_capacity,
                .get_size = law_table_get_size,
                .set_size = law_table_set_size,
                .get_element_size = law_table_get_element_size},
        .node_iface = {
                .get_next = law_table_node_get_next,
                .set_next = law_table_node_set_next
        },
        .get_key = law_table_node_get_key,
        .get_equals = law_table_get_equals,
        .get_hash = law_table_get_hash
};

law_table_t *law_table_create(const size_t capacity)
{
        law_table_t *map = malloc(sizeof(law_table_t));
        if(!map) return NULL;

        if(!pmt_hm_create(&law_table_iface, map, capacity)) {
                free(map);
                return NULL;
        }

        return map;
}

void law_table_destroy(law_table_t *map)
{
        pmt_hm_iter_t iter;
        pmt_hm_entries(&law_table_iface, map, &iter);

        void *node;
        while(pmt_hm_next(&law_table_iface, map, &node)) {
                free(node);
        }

        pmt_hm_destroy(&law_table_iface, map);
        free(map);
}

int law_table_insert(law_table_t *table, law_id_t id, void *pointer)
{
        law_table_node_t *node = malloc(sizeof(law_table_node_t));
        
        const int result = pmt_hm_insert(&law_table_iface, table, node);

        if(result != PMT_HM_SUCCESS) {
                free(node);
        }

        return result;
}

void *law_table_lookup(law_table_t *table, law_id_t id)
{
        law_table_node_t *node = pmt_hm_lookup(&law_table_iface, table, &id);
        if(!node) return NULL; 

        return node->pointer;
}

void *law_table_remove(law_table_t *table, law_id_t id)
{
        law_table_node_t *node = pmt_hm_remove(&law_table_iface, table, &id);
        if(!node) return false;

        void *pointer = node->pointer;
        free(node);
        
        return pointer;
}

void law_table_entries(law_table_t *table, pmt_hm_iter_t *iter)
{
        pmt_hm_entries(&law_table_iface, table, iter);
}

bool law_table_next(pmt_hm_iter_t *iter, law_id_t *id, void **pointer)
{
        law_table_node_t *node;
        if(!pmt_hm_next(&law_table_iface, iter, &node))
                return false;

        if(id) *id = node->id;
        if(pointer) *pointer = node->pointer;

        return true;
}

bool law_table_is_next(pmt_hm_iter_t *iter)
{
        return pmt_hm_is_next(&law_table_iface, iter);
}

size_t law_table_size(law_table_t *table)
{
        return table->size;
}

size_t law_table_max_size(law_table_t *table)
{
        return ((table->capacity * 3) / 4) - 1;
}