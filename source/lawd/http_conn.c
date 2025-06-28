
#include "lawd/http_conn.h"
#include "lawd/private/http_conn.h"
#include "lawd/buffer.h"
#include "lawd/time.h"
#include "lawd/error.h"
#include "lawd/server.h"
#include "pgenc/lang.h"
#include <errno.h>
#include <unistd.h>
#include <openssl/err.h>

typedef struct law_htc_args2 {
        law_htconn_t *conn;
        size_t nbytes;
} law_htc_args2_t;

sel_err_t law_htc_close(law_htconn_t *conn)
{
        close(conn->socket);
        return LAW_ERR_OK;
}

intptr_t law_htc_read_data(law_htconn_t *conn)
{
        struct pgc_buf *in = conn->in;

        const size_t 
                block = pgc_buf_end(in) - pgc_buf_tell(in),
                avail = pgc_buf_max(in) - block;
        
        if(!avail) {
                return LAW_ERR_OOB;
        } 

        switch(conn->security) {
                case LAW_HTC_UNSECURED:
                        return law_buf_read(in, conn->socket, avail);
                case LAW_HTC_SSL: 
                        return law_buf_read_SSL(in, conn->ssl, avail);
                default: 
                        return SEL_HALT();
        }
}

sel_err_t law_htc_ensure_input(law_htconn_t *conn, const size_t nbytes)
{
        struct pgc_buf *in = conn->in;

        const size_t 
                offset = pgc_buf_tell(in),
                end = pgc_buf_end(in);

        if(((end - offset) + nbytes) > pgc_buf_max(in)) {
                return LAW_ERR_OOB;
        }

        if((pgc_buf_end(in) - offset) >= nbytes) {
                return LAW_ERR_OK;
        }

        const intptr_t result = law_htc_read_data(conn);
        if(!(result > 0)) {
                return (sel_err_t)result;
        }

        return (pgc_buf_end(in) - offset) >= nbytes ? 
                LAW_ERR_OK : 
                LAW_ERR_WANTR;
}

static sel_err_t law_htc_ensure_input_cb(int fd, void *state)
{
        law_htc_args2_t *args = state;
        return law_htc_ensure_input(args->conn, args->nbytes);
}

sel_err_t law_htc_ensure_input_sync(
        law_worker_t *worker,
        law_time_t timeout,
        law_htconn_t *conn, 
        const size_t nbytes)
{
        law_htc_args2_t args = { .conn = conn, .nbytes = nbytes };
        return law_sync(
                worker, 
                timeout, 
                law_htc_ensure_input_cb, 
                conn->socket, 
                &args);
}

intptr_t law_htc_write_data(law_htconn_t *conn)
{
        struct pgc_buf *out = conn->out;

        const size_t block = pgc_buf_end(out) - pgc_buf_tell(out);

        if(!block) {
                return LAW_ERR_OK;
        }
        
        switch(conn->security) {
                case LAW_HTC_UNSECURED:
                        return law_buf_write(out, conn->socket, block);
                case LAW_HTC_SSL:
                        return law_buf_write_SSL(out, conn->ssl, block);
                default: 
                        return SEL_HALT();
        }
}

sel_err_t law_htc_ensure_output(law_htconn_t *conn, const size_t nbytes)
{
        struct pgc_buf *out = conn->out;

        const size_t 
                end = pgc_buf_end(out),
                max = pgc_buf_max(out);

        size_t 
                offset = pgc_buf_tell(out),
                unwritten = end - offset;

        SEL_ASSERT(unwritten <= max);

        if(nbytes > max) {
                return LAW_ERR_OOB;
        }

        if((max - unwritten) >= nbytes) {
                return LAW_ERR_OK;
        }

        const intptr_t result = law_htc_write_data(conn);
        if(!(result > 0)) {
                return (sel_err_t)result;
        }
       
        offset = pgc_buf_tell(out);
        unwritten = end - offset;

        SEL_ASSERT(unwritten <= max);

        if((max - unwritten) >= nbytes) {
                return LAW_ERR_OK;
        } else {
                return LAW_ERR_WANTW;
        }
}

static sel_err_t law_htc_ensure_output_cb(int fd, void *state)
{
        law_htc_args2_t *args = state;
        return law_htc_ensure_output(args->conn, args->nbytes);
}

sel_err_t law_htc_ensure_output_sync(
        law_worker_t *worker,
        law_time_t timeout,
        law_htconn_t *conn, 
        const size_t nbytes)
{
        law_htc_args2_t args = { .conn = conn, .nbytes = nbytes };
        return law_sync(
                worker, 
                timeout, 
                law_htc_ensure_output_cb, 
                conn->socket, 
                &args);
}

sel_err_t law_htc_flush(law_htconn_t *conn)
{
        struct pgc_buf *out = conn->out;

        size_t offset = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);

        if((end - offset) == 0) {
                return LAW_ERR_OK;
        }

        const intptr_t result = law_htc_write_data(conn);
        if(!(result > 0)) {
                return (sel_err_t)result;
        }

        offset = pgc_buf_tell(out);

        return (end - offset) == 0 ? 
                LAW_ERR_OK : 
                LAW_ERR_WANTW;
}

static sel_err_t law_htc_flush_cb(int fd, void *state)
{
        return law_htc_flush((law_htconn_t*)state);
}

sel_err_t law_htc_flush_sync(
        law_worker_t *worker, 
        law_time_t timeout, 
        law_htconn_t *conn)
{
        return law_sync(
                worker, 
                timeout, 
                law_htc_flush_cb, 
                conn->socket, 
                conn);
}

sel_err_t law_htc_read_scan(
        law_htconn_t *conn, 
        void *delim, 
        const size_t len)
{
        struct pgc_buf *in = conn->in;

        const intptr_t result = law_htc_read_data(conn);

        if(pgc_buf_max(in) < len) {
                return LAW_ERR_OOB;
        }
        
        if(!(result > 0)) {
                switch(result) {
                        case LAW_ERR_WANTW:
                        case LAW_ERR_WANTR: 
                        case LAW_ERR_EOF:
                        case LAW_ERR_OOB: 
                                break;
                        default:
                                return LAW_ERR_PUSH(
                                        (sel_err_t)result,
                                         "law_htc_read_data");
                }
        }

        if(pgc_buf_scan(in, delim, len) == LAW_ERR_OK) {
                return LAW_ERR_OK;
        } 

        if(result > 0) {
                return LAW_ERR_WANTR;
        } 

        return (sel_err_t)result;
}

sel_err_t law_htc_ssl_accept(law_htconn_t *conn, SSL_CTX *ssl_ctx)
{
        law_err_clear();
        ERR_clear_error();

        SSL *ssl = SSL_new(ssl_ctx);
        if(!ssl) 
                return LAW_ERR_PUSH(LAW_ERR_SSL, "SSL_new");

        ERR_clear_error();
        int ssl_err = SSL_set_fd(ssl, conn->socket);
        if(ssl_err == 0) {
                SSL_free(ssl);
                return LAW_ERR_PUSH(LAW_ERR_SSL, "SSL_set_fd");
        }

        conn->security = LAW_HTC_SSL;
        conn->ssl = ssl;

        ERR_clear_error();
        ssl_err = SSL_accept(ssl);

        if(ssl_err == 0) {
                /* SSL was shut down gracefully. */
                SSL_free(ssl);
                return LAW_ERR_EOF;
        } else if(ssl_err < 0) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WANTR;
                        case SSL_ERROR_WANT_WRITE: 
                                return LAW_ERR_WANTW;
                        case SSL_ERROR_SYSCALL:
                                SSL_free(ssl);
                                return LAW_ERR_PUSH(
                                        LAW_ERR_SYS, 
                                        "SSL_accept");
                        default:
                                SSL_free(ssl);
                                return LAW_ERR_PUSH(
                                        LAW_ERR_SSL,
                                        "SSL_accept");
                }
        } else {
                return LAW_ERR_OK;
        }
}

sel_err_t law_htc_ssl_shutdown(law_htconn_t *conn)
{
        law_err_clear();

        SSL *ssl = conn->ssl;
        SEL_ASSERT(ssl);

        ERR_clear_error();
        int ssl_err = SSL_shutdown(ssl);

        if(ssl_err == 0) {
                return LAW_ERR_WANTR;
        } else if(ssl_err < 0) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WANTR;
                        case SSL_ERROR_WANT_WRITE: 
                                return LAW_ERR_WANTW;
                        case SSL_ERROR_SYSCALL: 
                                return LAW_ERR_PUSH(
                                        LAW_ERR_SYS,
                                        "SSL_shutdown");
                        default: 
                                return LAW_ERR_PUSH(
                                        LAW_ERR_SSL,
                                        "SSL_shutdown");
                }
        } else {
                return LAW_ERR_OK; 
        }
}

static sel_err_t law_htc_ssl_shutdown_cb(int fd, void *state)
{
        return law_htc_ssl_shutdown((law_htconn_t*)state);
}

sel_err_t law_htc_ssl_shutdown_sync(
        law_worker_t *worker, 
        law_time_t timeout,
        law_htconn_t *conn)
{
        return law_sync(
                worker, 
                timeout, 
                law_htc_ssl_shutdown_cb, 
                conn->socket, 
                conn);
}

void law_htc_ssl_free(law_htconn_t *conn)
{
        SSL_free(conn->ssl);
}
