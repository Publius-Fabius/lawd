
#include "lawd/server.h"

struct law_tasks;
struct law_tasks *law_tasks_create();
void law_tasks_destroy(struct law_tasks *ts);
struct law_tasks *law_tasks_reset(struct law_tasks *ts);
struct law_tasks *law_tasks_grow(struct law_tasks *ts);
size_t law_tasks_size(struct law_tasks *ts);
struct law_task **law_tasks_array(struct law_tasks *ts);
struct law_tasks *law_tasks_add(struct law_tasks *ts, struct law_task *t);
struct law_worker *law_worker_create(
        struct law_server *s, const unsigned int id);
void law_worker_destroy(struct law_worker *w);
struct law_task *law_task_create(
        law_srv_call_t callback,
        struct law_data *data,
        const size_t stack_length,
        const size_t stack_guard);
void law_task_destroy(struct law_task *task);

void test_tasks()
{
        SEL_INFO();
        struct law_tasks *tasks = law_tasks_create();

        law_tasks_add(tasks, (struct law_task*)0);
        SEL_TEST(law_tasks_size(tasks) == 1);
        law_tasks_add(tasks, (struct law_task*)1);
        SEL_TEST(law_tasks_size(tasks) == 2);
        law_tasks_add(tasks, (struct law_task*)2);
        SEL_TEST(law_tasks_size(tasks) == 3);

        for(size_t x = 0; x < 3; ++x) {
                SEL_TEST(law_tasks_array(tasks)[x] == (struct law_task*)x);
        }

        law_tasks_reset(tasks);
        SEL_TEST(law_tasks_size(tasks) == 0);
        for(size_t x = 0; x < 3; ++x) {
                SEL_TEST(law_tasks_array(tasks)[x] == (struct law_task*)0);
        }

        law_tasks_destroy(tasks);
}

void test_srv_create()
{
        SEL_INFO();
        struct law_srv_cfg cfg;

        cfg.workers = 0;
        struct law_server *server = law_srv_create(&cfg);
        SEL_TEST(!server);
        law_srv_destroy(server);
        
        cfg.workers = 0xFFFFFF;
        server = law_srv_create(&cfg);
        SEL_TEST(!server);
        law_srv_destroy(server);

        cfg = law_srv_sanity();
        server = law_srv_create(&cfg);
        SEL_TEST(server);

        law_srv_destroy(server);
}

void test_worker()
{
        SEL_INFO();

        struct law_srv_cfg cfg = law_srv_sanity();
        struct law_server *server = law_srv_create(&cfg);
        struct law_worker *worker = law_worker_create(server, 0);
        law_worker_destroy(worker);
        law_srv_destroy(server);
}

void test_task_create()
{
        SEL_INFO();

        struct law_data d;
        d.u.u32 = 123;

        struct law_task *task = law_task_create(NULL, &d, 4096, 4096);

        law_task_destroy(task);
}

int main(int argc, char **args)
{
        SEL_INFO();
        test_tasks();
        test_srv_create();
        test_worker();
        test_task_create();
}