#ifndef LAWD_HTTP_CONN_H
#define LAWD_HTTP_CONN_H

#include "lawd/server.h"
#include "pgenc/parser.h"
#include "pgenc/ast.h"
#include <openssl/ssl.h>

/** HTTP Connection Security Mode */
enum law_htc_sec {
        LAW_HTC_SSL             = 1,            /** Use OpenSSL Socket IO */
        LAW_HTC_UNSECURED       = 2,            /** Use System Socket IO */
};

/** HTTP Connection */
struct law_htconn {
        int socket;
        int security;
        SSL *ssl;
        struct pgc_buf *in, *out;
};

/** Close the connection. */
sel_err_t law_htc_close(struct law_htconn *conn);

/** 
 * Read data into the connection's input buffer.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_OOB - Buffer is full.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_read_data(struct law_htconn *conn);

/**
 * Ensure nbytes of input are available in the input buffer.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_OOB - Buffer out of bounds.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ensure_input(struct law_htconn *conn, const size_t nbytes);

/**
 * Ensure nbytes of available space in the output buffer.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_OOB - Buffer out of bounds.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ensure_output(struct law_htconn *conn, const size_t nbytes);

/**
 * Write data from the output buffer.
 * 
 * LAW_ERR_WNTR - Wants to read
 * LAW_ERR_WNTW - Wants to write
 * LAW_ERR_SYS - System error
 * LAW_ERR_SSL - SSL error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_write_data(struct law_htconn *conn);

/**
 * Flush the request's output buffer.
 * 
 * LAW_ERR_WNTR - Wants to read
 * LAW_ERR_WNTW - Wants to write
 * LAW_ERR_SYS - System error
 * LAW_ERR_SSL - SSL error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_flush(struct law_htconn *conn);

/** Read an HTTP message head (non-blocking). */
sel_err_t law_htc_read_head(
        struct law_htconn *conn,
        const struct pgc_par *parser,
        struct pgc_stk *stack,
        struct pgc_stk *heap,
        struct pgc_ast_lst **head);

/**
 * Accept an SSL connection.  This function should only be called once even 
 * if it returns LAW_ERR_WNTR or LAW_ERR_WNTW.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ssl_accept(struct law_htconn *conn, SSL_CTX *ssl_ctx);

/**
 * Shutdown an SSL connection and free its resources.  This function should
 * be called multiple times until it returns LAW_ERR_OK in order to fully
 * complete the two way SSL shutdown procedure.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ssl_shutdown(struct law_htconn *conn);

/** Free any resources acquired by ssl_accept. */
sel_err_t law_htc_ssl_free(struct law_htconn *conn);

#endif