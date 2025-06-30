
#include "lawd/http_server.h"
#include "lawd/private/http_server.h"
#include "lawd/error.h"
#include <unistd.h>
#include <fcntl.h>

law_hts_buf_t *law_hts_buf_create(const size_t length);
void law_hts_buf_destroy(law_hts_buf_t *buf);

void test_buf_create_destroy()
{
        SEL_INFO();

        law_hts_buf_t *buf = law_hts_buf_create(4096);
        SEL_ASSERT(buf);
        SEL_ASSERT(law_smem_length(buf->mapping) == 4096);
        SEL_ASSERT(buf->buffer.addr == law_smem_address(buf->mapping));

        law_hts_buf_destroy(buf);
}

law_hts_stk_t *law_hts_stk_create(const size_t length);
void law_hts_stk_destroy(law_hts_stk_t *buf);

void test_stk_create_destroy()
{
        SEL_INFO();

        law_hts_stk_t *buf = law_hts_stk_create(4096);
        SEL_ASSERT(buf);
        SEL_ASSERT(law_smem_length(buf->mapping) == 4096);
        SEL_ASSERT(buf->stack.base == law_smem_address(buf->mapping));

        law_hts_stk_destroy(buf);
}

void law_hts_buf_pool_free(law_hts_pool_t *pool);
law_hts_pool_t *law_hts_buf_pool_init(
        law_hts_pool_t *pool,
        const size_t pool_size, 
        const size_t buf_length);
law_hts_buf_t *law_hts_buf_pool_pop(law_hts_pool_t *pool);

void test_buf_pool_init()
{
        SEL_INFO();

        law_hts_pool_t pool;
        law_hts_buf_pool_init(&pool, 4, 4096);

        law_hts_buf_t *buf = NULL;

        for(int x = 0; x < 4; ++x) {
                SEL_TEST((buf = law_hts_buf_pool_pop(&pool)));
                SEL_TEST(law_smem_length(buf->mapping) == 4096);
                law_hts_buf_destroy(buf);
        }

        SEL_TEST(pool.list == NULL);
        
        law_hts_buf_pool_free(&pool);

        law_hts_buf_pool_init(&pool, 4, 4096);
        law_hts_buf_pool_free(&pool);
}

void law_hts_stk_pool_free(law_hts_pool_t *pool);
law_hts_pool_t *law_hts_stk_pool_init(
        law_hts_pool_t *pool,
        const size_t pool_size, 
        const size_t stk_length);
law_hts_stk_t *law_hts_stk_pool_pop(law_hts_pool_t *pool);

void test_stk_pool_init()
{
        SEL_INFO();

        law_hts_pool_t pool;
        law_hts_stk_pool_init(&pool, 4, 4096);

        law_hts_stk_t *stk = NULL;

        for(int x = 0; x < 4; ++x) {
                SEL_TEST((stk = law_hts_stk_pool_pop(&pool)));
                SEL_TEST(law_smem_length(stk->mapping) == 4096);
                law_hts_stk_destroy(stk);
        }

        SEL_TEST(pool.list == NULL);
        
        law_hts_stk_pool_free(&pool);

        law_hts_stk_pool_init(&pool, 4, 4096);
        law_hts_stk_pool_free(&pool);
}

law_hts_pool_group_t *law_hts_pool_group_create(
        law_server_cfg_t *server_cfg, 
        law_htserver_cfg_t *htserver_cfg);
void law_hts_pool_group_destroy(law_hts_pool_group_t *grp);

void test_pool_group_create_destroy()
{
        SEL_INFO();

        law_server_cfg_t srv_cfg = law_server_sanity();
        srv_cfg.workers = 4;
        srv_cfg.worker_tasks = 3;
        law_htserver_cfg_t hts_cfg = law_htserver_sanity();
        hts_cfg.in = 4096;
        hts_cfg.out = 4096;
        hts_cfg.heap = 4096;
        hts_cfg.stack = 4096;
        law_hts_pool_group_t *group = law_hts_pool_group_create(
                &srv_cfg, &hts_cfg);
        law_hts_pool_group_destroy(group);
}

law_hts_pool_group_t **law_hts_pool_groups_create(
        law_server_cfg_t *srv_cfg, 
        law_htserver_cfg_t *hts_cfg);
void law_hts_pool_groups_destroy(
        law_hts_pool_group_t **groups, 
        const int ngroups);

void test_pool_groups_create_destroy()
{
        SEL_INFO();

        law_server_cfg_t srv_cfg = law_server_sanity();
        srv_cfg.workers = 4;
        srv_cfg.worker_tasks = 3;
        law_htserver_cfg_t hts_cfg = law_htserver_sanity();
        hts_cfg.in = 4096;
        hts_cfg.out = 4096;
        hts_cfg.heap = 4096;
        hts_cfg.stack = 4096;

        law_hts_pool_group_t **groups = law_hts_pool_groups_create(
                &srv_cfg, &hts_cfg);
        
        law_hts_pool_groups_destroy(groups, srv_cfg.workers);
}

law_htserver_t *law_htserver_create(
        law_server_cfg_t *srv_cfg,
        law_htserver_cfg_t *hts_cfg);
void law_htserver_destroy(law_htserver_t *server);

void test_htserver_create_destroy()
{
        SEL_INFO();

        law_server_cfg_t srv_cfg = law_server_sanity();
        srv_cfg.workers = 4;
        srv_cfg.worker_tasks = 3;
        law_htserver_cfg_t hts_cfg = law_htserver_sanity();
        hts_cfg.in = 4096;
        hts_cfg.out = 4096;
        hts_cfg.heap = 4096;
        hts_cfg.stack = 4096;

        law_htserver_t *srv = law_htserver_create(&srv_cfg, &hts_cfg);

        law_htserver_destroy(srv);
}

void test_read_reqline()
{
        SEL_INFO();

        char in_bs[256];
        struct pgc_buf in;
        pgc_buf_init(&in, in_bs, 256, 0);

        char stack_bs[512];
        struct pgc_stk stack;
        pgc_stk_init(&stack, stack_bs, 512);
        
        char heap_bs[0x10000];
        struct pgc_stk heap;
        pgc_stk_init(&heap, heap_bs, 0x10000);

        int fds[2] = { 0 };
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_hts_req_t req;
        
        req.conn.in = &in;
        req.conn.security = LAW_HTC_UNSECURED;
        req.conn.socket = fds[0];
        req.heap = &heap;
        req.stack = &stack;
        
        write(fds[1], "POST / HTTP/1.1\r\n", 17);

        law_hts_reqline_t reqline;
        
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, pgc_buf_tell(&in)) == LAW_ERR_OK);
        SEL_ASSERT(strcmp("POST", reqline.method) == 0);
        SEL_ASSERT(strcmp("/", reqline.target.path) == 0);
        SEL_ASSERT(strcmp("HTTP/1.1", reqline.version) == 0);
        SEL_ASSERT(pgc_buf_tell(&in) == 17);

        size_t base = pgc_buf_tell(&in);

        for(int x = 0; x < 255; ++x) {
                write(fds[1], "w", 1);
                SEL_ASSERT(law_hts_read_reqline(
                        &req, &reqline, base) == LAW_ERR_WANTR);
        }

        write(fds[1], "w", 1);
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, base) == LAW_ERR_OOB);

        pgc_buf_zero(&in);
        write(fds[1], "POST / HTTX/1.1\r\n", 17);
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, pgc_buf_tell(&in)) == LAW_ERR_SYN);
                
        pgc_buf_zero(&in);
        
        for(int x = 0; x < 254; ++x) {
                write(fds[1], "w", 1);
                SEL_ASSERT(law_hts_read_reqline(
                        &req, &reqline, 0) == LAW_ERR_WANTR);
        }

        write(fds[1], "\r\n", 2);
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, 0) == LAW_ERR_SYN);
        
        pgc_buf_zero(&in);
        write(fds[1], "POST / HTTP/1.1 \r\n", 18);
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, pgc_buf_tell(&in)) == LAW_ERR_SYN);

        pgc_buf_zero(&in);
        write(fds[1], "GET http://h.com/path?query HTTP/1.1\r\n", 39);
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, pgc_buf_tell(&in)) == LAW_ERR_OK);
        SEL_ASSERT(strcmp(reqline.method, "GET") == 0);
        SEL_ASSERT(strcmp(reqline.version, "HTTP/1.1") == 0);
        SEL_ASSERT(strcmp(reqline.target.scheme, "http") == 0);
        SEL_ASSERT(strcmp(reqline.target.host, "h.com") == 0);
        SEL_ASSERT(strcmp(reqline.target.path, "/path") == 0);
        SEL_ASSERT(strcmp(reqline.target.query, "query") == 0);

        pgc_buf_zero(&in);
        heap.offset = 0;
        write(fds[1], "GET http://h.com/path?query HTTP/1.1\r\n", 39);
        SEL_ASSERT(law_hts_read_reqline(
                &req, &reqline, pgc_buf_tell(&in)) == LAW_ERR_OOM);

        close(fds[0]);
        close(fds[1]);
}

int main(int argc, char **args)
{
        SEL_INFO();

        law_err_init();

        test_buf_create_destroy();
        test_stk_create_destroy();
        test_buf_pool_init();
        test_stk_pool_init();
        test_pool_group_create_destroy();
        test_pool_groups_create_destroy();
        test_htserver_create_destroy();
        test_read_reqline();
}