
#include "lawd/server.h"
#include "lawd/private/server.h"
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

sel_err_t law_msg_queue_open(law_msg_queue_t *queue);
void law_msg_queue_close(law_msg_queue_t *queue);

void test_msg_queue_open_close()
{
        law_msg_queue_t queue;
        law_msg_queue_open(&queue);
        law_msg_queue_close(&queue);
}

sel_err_t law_msg_queue_push(law_msg_queue_t *queue, law_msg_t *msg);

void test_msg_queue_push()
{
        law_msg_queue_t queue;
        law_msg_queue_open(&queue);

        law_msg_t msg = { .type = 1, .data = { .u64 = 123 } };
        memset(&msg, 0, sizeof(law_msg_t));

        assert(law_msg_queue_push(&queue, &msg) == LAW_ERR_OK);
        assert(law_msg_queue_push(&queue, &msg) == LAW_ERR_OK);
        assert(law_msg_queue_push(&queue, &msg) == LAW_ERR_OK);

        law_msg_queue_close(&queue);
}

sel_err_t law_msg_queue_pop(law_msg_queue_t *queue, law_msg_t *msg);

void test_msg_queue_pop()
{
        law_msg_queue_t queue;
        law_msg_queue_open(&queue);

        law_msg_t msg;
        memset(&msg, 0, sizeof(law_msg_t));

        msg.type = 1;
        msg.data.u64 = 1;
        assert(law_msg_queue_push(&queue, &msg) == LAW_ERR_OK);

        msg.type = 2;
        msg.data.u64 = 2;
        assert(law_msg_queue_push(&queue, &msg) == LAW_ERR_OK);

        msg.type = 3;
        msg.data.u64 = 3;
        assert(law_msg_queue_push(&queue, &msg) == LAW_ERR_OK);

        assert(law_msg_queue_pop(&queue, &msg) == LAW_ERR_OK);
        assert(msg.type == 1 && msg.data.u64 == 1);

        assert(law_msg_queue_pop(&queue, &msg) == LAW_ERR_OK);
        assert(msg.type == 2 && msg.data.u64 == 2);

        assert(law_msg_queue_pop(&queue, &msg) == LAW_ERR_OK);
        assert(msg.type == 3 && msg.data.u64 == 3);

        assert(law_msg_queue_pop(&queue, &msg) == LAW_ERR_WNTR);

        law_msg_queue_close(&queue);
}

sel_err_t law_task_queue_open(law_task_queue_t *queue);

void law_task_queue_close(law_task_queue_t *queue);

void test_task_queue_open_close()
{
        law_task_queue_t queue;
        law_task_queue_open(&queue);
        law_task_queue_close(&queue);
}

sel_err_t law_task_queue_push(law_task_queue_t *queue, law_task_t *task);

void test_task_queue_push()
{
        law_task_queue_t queue;
        law_task_queue_open(&queue);

        law_task_t *ptr = NULL;

        assert(law_task_queue_push(&queue, ptr) == LAW_ERR_OK);
        assert(law_task_queue_push(&queue, ptr) == LAW_ERR_OK);
        assert(law_task_queue_push(&queue, ptr) == LAW_ERR_OK);

        law_task_queue_close(&queue);
}

sel_err_t law_task_queue_pop(law_task_queue_t *queue, law_task_t **task);

void test_task_queue_pop()
{
        law_task_queue_t queue;
        law_task_queue_open(&queue);

        law_task_t *ptr = (law_task_t*)1;
        assert(law_task_queue_push(&queue, ptr) == LAW_ERR_OK);

        ptr = (law_task_t*)2;
        assert(law_task_queue_push(&queue, ptr) == LAW_ERR_OK);

        ptr = (law_task_t*)3;
        assert(law_task_queue_push(&queue, ptr) == LAW_ERR_OK);

        assert(law_task_queue_pop(&queue, &ptr) == LAW_ERR_OK);
        assert(ptr == (law_task_t*)1);

        assert(law_task_queue_pop(&queue, &ptr) == LAW_ERR_OK);
        assert(ptr == (law_task_t*)2);

        assert(law_task_queue_pop(&queue, &ptr) == LAW_ERR_OK);
        assert(ptr == (law_task_t*)3);

        law_task_queue_close(&queue);
}

law_task_t *law_task_create(size_t stack_length, size_t stack_guard);
void law_task_destroy(law_task_t *task) ;

void test_task_create_destroy()
{
        law_task_t *task = law_task_create(4096, 4096);

        law_task_destroy(task);
}

law_task_pool_t *law_task_pool_create(law_server_config_t *cfg);
void law_task_pool_destroy(law_task_pool_t *pool);
bool law_task_pool_is_full(law_task_pool_t *pool);

void test_task_pool_create_destroy()
{
        law_server_config_t cfg = law_server_sanity();

        law_task_pool_t *pool = law_task_pool_create(&cfg);

        assert(law_task_pool_is_full(pool));

        law_task_pool_destroy(pool);
}

law_task_t *law_task_pool_pop(law_task_pool_t *pool);
void law_task_pool_push(law_task_pool_t *pool, law_task_t *task);
size_t law_task_pool_size(law_task_pool_t *pool);
bool law_task_pool_is_empty(law_task_pool_t *pool);

void test_task_pool_pop_push()
{
        law_server_config_t cfg = law_server_sanity();
        cfg.workers = 1;
        cfg.worker_tasks = 2;

        law_task_pool_t *pool = law_task_pool_create(&cfg);
        law_task_t *task;

        assert(law_task_pool_is_full(pool));
        assert(law_task_pool_size(pool) == 2);

        assert((task = law_task_pool_pop(pool)));
        assert(!law_task_pool_is_empty(pool));
        assert(!law_task_pool_is_full(pool));
        assert(law_task_pool_size(pool) == 1);
        law_task_destroy(task);

        assert((task = law_task_pool_pop(pool)));
        assert(law_task_pool_is_empty(pool));
        assert(!law_task_pool_is_full(pool));
        assert(law_task_pool_size(pool) == 0);
        law_task_destroy(task);

        law_task_pool_destroy(pool);
}

void law_ready_set_init(law_ready_set_t *set);
bool law_ready_set_push(law_ready_set_t *set, law_task_t *task);
law_task_t *law_ready_set_pop(law_ready_set_t *set);

void test_ready_set_push_pop()
{
        law_ready_set_t ready;

        law_server_config_t cfg = law_server_sanity();
        cfg.workers = 1;
        cfg.worker_tasks = 2;

        law_task_pool_t *pool = law_task_pool_create(&cfg);

        law_ready_set_init(&ready);
        assert(law_ready_set_push(&ready, law_task_pool_pop(pool)));
        assert(law_ready_set_push(&ready, law_task_pool_pop(pool)));
        
        law_task_pool_push(pool, law_ready_set_pop(&ready));
        law_task_pool_push(pool, law_ready_set_pop(&ready));

        law_task_pool_destroy(pool);
}

law_worker_t *law_worker_create(law_server_t *server, const int id);

void test_server_create_destroy()
{
        law_server_config_t cfg = law_server_sanity();
        cfg.workers = 2;
        cfg.worker_tasks = 2;

        law_server_t *server = law_server_create(&cfg);

        law_server_destroy(server);
}


uint64_t law_slot_encode(law_slot_t *slot);

void law_slot_decode(uint64_t encoding, law_slot_t *slot);

void test_slot_encode_decode()
{
        law_slot_t slot = { .id = 0xABCDEFABCDEFABUL, .index = 5 };

        uint64_t code = law_slot_encode(&slot);

        slot.id = 0;
        slot.index = 0;

        law_slot_decode(code, &slot);

        assert(slot.id == 0xABCDEFABCDEFABUL);
        assert(slot.index == 5);
}

int main(int argc, char **args)
{
        SEL_INFO();

        test_msg_queue_open_close();
        test_msg_queue_push();
        test_msg_queue_pop();

        test_task_queue_open_close();
        test_task_queue_push();
        test_task_queue_pop();

        test_task_create_destroy();

        test_task_pool_create_destroy();
        test_task_pool_pop_push();

        test_ready_set_push_pop();

        test_server_create_destroy();

        test_slot_encode_decode();
}