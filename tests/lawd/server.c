
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
        law_callback_t callback,
        struct law_data data,
        const size_t stack_length,
        const size_t stack_guard);
void law_task_destroy(struct law_task *task);

sel_err_t test_call(struct law_worker *worker, struct law_data data)
{
        return LAW_ERR_OK;
}

void test_tasks()
{
        SEL_INFO();

        struct law_data d;

        struct law_tasks *tasks = law_tasks_create();

        struct law_task *t0 = law_task_create(test_call, d, 4096, 4096);
        struct law_task *t1 = law_task_create(test_call, d, 4096, 4096);
        struct law_task *t2 = law_task_create(test_call, d, 4096, 4096);

        law_tasks_add(tasks, t0);
        law_tasks_add(tasks, t1);
        law_tasks_add(tasks, t2);

        SEL_TEST(law_tasks_size(tasks) == 3);

        struct law_task **array = law_tasks_array(tasks);

        SEL_TEST(array[0] == t0);
        SEL_TEST(array[1] == t1);
        SEL_TEST(array[2] == t2);

        law_tasks_reset(tasks);
        SEL_TEST(law_tasks_size(tasks) == 0);

        law_task_destroy(t0);
        law_task_destroy(t1);
        law_task_destroy(t2);

        law_tasks_destroy(tasks);
}

void test_srv_create()
{
        SEL_INFO();
        struct law_server_config cfg = law_server_sanity();
        cfg.workers = 8;

        struct law_server *server = law_server_create(&cfg);
        SEL_TEST(server);
        law_server_destroy(server);
}

void test_worker()
{
        SEL_INFO();

        struct law_server_config cfg = law_server_sanity();
        struct law_server *server = law_server_create(&cfg);
        struct law_worker *worker = law_worker_create(server, 0);
        law_worker_destroy(worker);
        law_server_destroy(server);
}

int main(int argc, char **args)
{
        SEL_INFO();
        test_tasks();
        test_srv_create();
        test_worker();
}