#ifndef LAWD_HTTP_CONN_H
#define LAWD_HTTP_CONN_H

#include "pgenc/buffer.h"
#include "lawd/server.h"
#include <openssl/ssl.h>

enum law_htc_security_mode {                    /** HTTP Security Mode */
        LAW_HTC_SSL             = 1,            /** OpenSSL Socket IO */
        LAW_HTC_UNSECURED       = 2,            /** System Socket IO */
};

/** HTTP Connection */
typedef struct law_htconn law_htconn_t;

/** 
 * Get the connection's security mode. 
 */
int law_htc_get_security(law_htconn_t *conn);

/** 
 * Get the connection's socket. 
 */
int law_htc_get_socket(law_htconn_t *conn);

/** 
 * Get the connection's SSL state. 
 */
SSL *law_htc_get_SSL(law_htconn_t *conn);

/** 
 * Get the connection's input buffer. 
 */
struct pgc_buf *law_htc_get_in(law_htconn_t *conn);

/** 
 * Get the connection's output buffer. 
 */
struct pgc_buf *law_htc_get_out(law_htconn_t *conn);

/**
 * Close the connection. 
 */
sel_err_t law_htc_close(law_htconn_t *conn);

/** 
 * Read data into the connection's input buffer.
 * 
 * LAW_ERR_OK
 * LAW_ERR_WANTR
 * LAW_ERR_WANTW
 * LAW_ERR_OOB
 * LAW_ERR_EOF 
 * LAW_ERR_SYS
 * LAW_ERR_SSL
 */
intptr_t law_htc_read_data(law_htconn_t *conn);

/**
 * Read data and scan input for the delimiter.  The offset of the input 
 * buffer will point to the end of the delimiter upon success.
 * 
 * LAW_ERR_OK
 * LAW_ERR_WANTR
 * LAW_ERR_WANTW
 * LAW_ERR_OOB
 * LAW_ERR_EOF 
 * LAW_ERR_SYS
 * LAW_ERR_SSL
 */
sel_err_t law_htc_read_scan(
        law_htconn_t *conn, 
        const size_t base,
        void *delim, 
        const size_t delim_len);

/**
 * Ensure nbytes of input are available in the input buffer.
 * 
 * LAW_ERR_WANTR - Wants to read.
 * LAW_ERR_WANTW - Wants to write.
 * LAW_ERR_OOB - Buffer out of bounds.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ensure_input(law_htconn_t *conn, const size_t nbytes);

/**
 * Ensure nbytes of input are available in the input buffer.
 * 
 * LAW_ERR_TIMEOUT - Timed Out
 * LAW_ERR_OOB - Buffer out of bounds.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ensure_input_sync(
        law_worker_t *worker,
        law_time_t timeout,
        law_htconn_t *conn, 
        const size_t nbytes);

/**
 * Ensure nbytes of available space in the output buffer.
 * 
 * LAW_ERR_WANTR - Wants to read.
 * LAW_ERR_WANTW - Wants to write.
 * LAW_ERR_OOB - Buffer out of bounds.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ensure_output(law_htconn_t *conn, const size_t nbytes);

/**
 * Ensure nbytes of available space in the output buffer.
 * 
 * LAW_ERR_TIMEOUT - Timed Out
 * LAW_ERR_OOB - Buffer out of bounds.
 * LAW_ERR_EOF - End of file. 
 * LAW_ERR_SYS - Socket IO. 
 * LAW_ERR_SSL - SSL error. 
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ensure_output_sync(
        law_worker_t *worker,
        law_time_t timeout,
        law_htconn_t *conn, 
        const size_t nbytes);

/**
 * Write data from the output buffer.
 * 
 * LAW_ERR_WNTR - Wants to read
 * LAW_ERR_WNTW - Wants to write
 * LAW_ERR_SYS - System error
 * LAW_ERR_SSL - SSL error
 * LAW_ERR_OK - All OK.
 */
intptr_t law_htc_write_data(law_htconn_t *conn);

/**
 * Flush the request's output buffer.
 * 
 * LAW_ERR_WANTR - Wants to read
 * LAW_ERR_WANTW - Wants to write
 * LAW_ERR_SYS - System error
 * LAW_ERR_SSL - SSL error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_flush(law_htconn_t *conn);

/**
 * Flush the request's output buffer.
 * 
 * LAW_ERR_TIMEOUT - Time Exceeded
 * LAW_ERR_SYS - System error
 * LAW_ERR_SSL - SSL error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_flush_sync(
        law_worker_t *worker, 
        law_time_t timeout,
        law_htconn_t *conn);

/**
 * Accept an SSL connection.  This function should only be called once even 
 * if it returns LAW_ERR_WNTR or LAW_ERR_WNTW.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_EOF - End of File Encountered
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ssl_accept(law_htconn_t *conn, SSL_CTX *ssl_ctx);

/**
 * Shutdown an SSL connection and free its resources.  This function should
 * be called multiple times until it returns LAW_ERR_OK in order to fully
 * complete the two way SSL shutdown procedure.
 * 
 * LAW_ERR_WNTR - Wants to read.
 * LAW_ERR_WNTW - Wants to write.
 * LAW_ERR_EOF - End of File
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ssl_shutdown(law_htconn_t *conn);

/**
 * Shutdown an SSL connection and free its resources.  This function should
 * be called multiple times until it returns LAW_ERR_OK in order to fully
 * complete the two way SSL shutdown procedure.
 * 
 * LAW_ERR_TIMEOUT - Time Exceeded
 * LAW_ERR_EOF - End of File
 * LAW_ERR_SSL - SSL error.
 * LAW_ERR_SYS - Syscall Error
 * LAW_ERR_OK - All OK.
 */
sel_err_t law_htc_ssl_shutdown_sync(
        law_worker_t *worker,
        law_time_t timeout,
        law_htconn_t *conn);

/** 
 * Free any resources acquired by ssl_accept. 
 */
void law_htc_ssl_free(law_htconn_t *conn);

#endif