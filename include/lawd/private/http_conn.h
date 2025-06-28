#ifndef LAWD_PRIVATE_HTTP_CONN_H
#define LAWD_PRIVATE_HTTP_CONN_H

#include "lawd/http_conn.h"
#include "pgenc/parser.h"
#include "pgenc/ast.h"
#include <openssl/ssl.h>

typedef struct law_htconn {
        int socket;
        int security;
        SSL *ssl;
        struct pgc_buf *in, *out;
} law_htconn_t;

sel_err_t law_htc_read_head(
        law_htconn_t *conn,
        const struct pgc_par *parser,
        struct pgc_stk *stack,
        struct pgc_stk *heap,
        struct pgc_ast_lst **head);

#endif