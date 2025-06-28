#ifndef LAWD_PRIVATE_HTTP_SERVER_H
#define LAWD_PRIVATE_HTTP_SERVER_H

#include "lawd/safemem.h"
#include "lawd/http_server.h"
#include "lawd/private/http_conn.h"
#include "pgenc/buffer.h"
#include "pgenc/stack.h"

typedef struct law_hts_buf {
        struct pgc_buf buffer;
        law_smem_t *mapping;
        struct law_hts_buf *next;
} law_hts_buf_t;

typedef struct law_hts_stk {
        struct pgc_stk stack;
        law_smem_t *mapping;
        struct law_hts_stk *next;
} law_hts_stk_t;

typedef struct law_hts_pool {
        void *list;
} law_hts_pool_t;

typedef struct law_hts_pool_group {
        law_hts_pool_t 
                in_pool,
                out_pool,
                stack_pool,
                heap_pool;
} law_hts_pool_group_t;

struct law_htserver {
        law_htserver_cfg_t cfg;
        SSL_CTX *ssl_ctx;
        law_hts_pool_group_t **groups;
        int workers;
};

struct law_hts_req {
        law_htconn_t conn;
        struct pgc_stk 
                *stack, 
                *heap;
        law_htserver_t *htserver;
};

#endif