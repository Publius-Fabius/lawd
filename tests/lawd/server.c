
#include "lawd/server.h"

struct law_srv_pfds;

struct law_srv_pfds *law_srv_pfds_create();

void law_srv_pfds_destroy(struct law_srv_pfds *ps);

struct law_srv_pfds *law_srv_pfds_grow(struct law_srv_pfds *ps);

struct pollfd *law_srv_pfds_take(struct law_srv_pfds *ps);

struct law_srv_pfds *law_srv_pfds_reset(struct law_srv_pfds *ps);

size_t law_srv_pfds_size(struct law_srv_pfds *ps);

size_t law_srv_pfds_capacity(struct law_srv_pfds *ps);

struct pollfd *law_srv_pfds_array(struct law_srv_pfds *ps);

struct law_srv_conn;

struct law_srv_conns *law_srv_conns_create();

void law_srv_conns_destroy(struct law_srv_conns *cs);

struct law_srv_conns *law_srv_conns_grow(struct law_srv_conns *cs);

struct law_srv_conn **law_srv_conns_take(struct law_srv_conns *cs);

struct law_srv_conns *law_srv_conns_reset(struct law_srv_conns *cs);

size_t law_srv_conns_size(struct law_srv_conns *cs);

size_t law_srv_conns_capacity(struct law_srv_conns *cs);

struct law_srv_conn ** law_srv_conns_array(struct law_srv_conns *cs);

void test_pfds()
{
        SEL_INFO();
        struct law_srv_pfds *pfds = law_srv_pfds_create();

        SEL_TEST(law_srv_pfds_capacity(pfds) == 1);
        SEL_TEST(law_srv_pfds_size(pfds) == 0);
        
        struct pollfd *pfd = law_srv_pfds_take(pfds);
        SEL_TEST(pfd->fd == 0);
        SEL_TEST(pfd->events == 0);
        SEL_TEST(pfd->revents == 0);
        pfd->fd = 1;
        pfd->events = 2;
        pfd->revents = 3;
        SEL_TEST(law_srv_pfds_size(pfds) == 1);

        pfd = law_srv_pfds_take(pfds);
        SEL_TEST(pfd->fd == 0);
        SEL_TEST(pfd->events == 0);
        SEL_TEST(pfd->revents == 0);
        pfd->fd = 4;
        pfd->events = 5;
        pfd->revents = 6;
        SEL_TEST(law_srv_pfds_size(pfds) == 2);

        pfd = law_srv_pfds_take(pfds);
        SEL_TEST(pfd->fd == 0);
        SEL_TEST(pfd->events == 0);
        SEL_TEST(pfd->revents == 0);
        pfd->fd = 7;
        pfd->events = 8;
        pfd->revents = 9;
        SEL_TEST(law_srv_pfds_size(pfds) == 3);

        pfd = law_srv_pfds_array(pfds);
        SEL_TEST(pfd->fd == 1);
        SEL_TEST(pfd->events == 2);
        SEL_TEST(pfd->revents == 3);

        pfd = law_srv_pfds_array(pfds) + 1;
        SEL_TEST(pfd->fd == 4);
        SEL_TEST(pfd->events == 5);
        SEL_TEST(pfd->revents == 6);

        pfd = law_srv_pfds_array(pfds) + 2;
        SEL_TEST(pfd->fd == 7);
        SEL_TEST(pfd->events == 8);
        SEL_TEST(pfd->revents == 9);

        law_srv_pfds_reset(pfds);
        SEL_TEST(law_srv_pfds_capacity(pfds) == 4);
        SEL_TEST(law_srv_pfds_size(pfds) == 0);

        pfd = law_srv_pfds_take(pfds);
        SEL_TEST(pfd->fd == 0);
        SEL_TEST(pfd->events == 0);
        SEL_TEST(pfd->revents == 0);

        law_srv_pfds_destroy(pfds);
}

void test_conns()
{
        SEL_INFO();

        struct law_srv_conns *cs = law_srv_conns_create();

        SEL_TEST(law_srv_conns_size(cs) == 0);
        SEL_TEST(law_srv_conns_capacity(cs) == 1);

        void **ptr = (void**)law_srv_conns_take(cs);
        SEL_TEST(*ptr == NULL);
        *ptr = (void*)1;
        SEL_TEST(law_srv_conns_size(cs) == 1);
        SEL_TEST(law_srv_conns_capacity(cs) == 1);

        ptr = (void**)law_srv_conns_take(cs);
        SEL_TEST(*ptr == NULL);
        *ptr = (void*)2;
        SEL_TEST(law_srv_conns_size(cs) == 2);
        SEL_TEST(law_srv_conns_capacity(cs) == 2);

        ptr = (void**)law_srv_conns_take(cs);
        SEL_TEST(*ptr == NULL);
        *ptr = (void*)3;
        SEL_TEST(law_srv_conns_size(cs) == 3);
        SEL_TEST(law_srv_conns_capacity(cs) == 4);

        intptr_t *iptr = (intptr_t*)law_srv_conns_array(cs);
        SEL_TEST(iptr[0] == 1);
        SEL_TEST(iptr[1] == 2);
        SEL_TEST(iptr[2] == 3);

        law_srv_conns_reset(cs);
        SEL_TEST(law_srv_conns_size(cs) == 0);
        SEL_TEST(law_srv_conns_capacity(cs) == 4);

        ptr = (void**)law_srv_conns_take(cs);
        SEL_TEST(law_srv_conns_size(cs) == 1);
        SEL_TEST(law_srv_conns_capacity(cs) == 4);
        SEL_TEST(*ptr == NULL);

        law_srv_conns_destroy(cs);
}

void test_create()
{
        SEL_INFO();
        
        static struct law_srv_cfg cfg;
        cfg.backlog = 10;
        cfg.gid = 1000;
        cfg.uid = 1000;
        cfg.guardlen = 0x1000;
        cfg.stacklen = 0x100000;
        cfg.prot = LAW_SRV_TCP;
        cfg.timeout = 5000;
        cfg.port = 80;
        cfg.maxconns = 10;
        cfg.tick = NULL;
        cfg.accept = NULL;

        struct law_srv *srv = law_srv_create(&cfg);
        law_srv_destroy(srv);
}

int main(int argc, char **args)
{
        SEL_INFO();
        test_pfds();
        test_conns();
        test_create();
}