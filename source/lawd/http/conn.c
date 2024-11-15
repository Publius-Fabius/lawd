
#include "lawd/http/conn.h"
#include "lawd/time.h"
#include "pgenc/lang.h"
#include <errno.h>
#include <unistd.h>
#include <openssl/err.h>

sel_err_t law_htc_close(struct law_htconn *conn)
{
        close(conn->socket);
        return LAW_ERR_OK;
}

static sel_err_t law_htc_read_SSL(
        struct pgc_buf *in,
        SSL *ssl,
        const size_t nbytes)
{
        int ssl_err;
        ERR_clear_error();
        const sel_err_t err = pgc_buf_sread(in, ssl, nbytes, &ssl_err);
        SEL_ASSERT(err != PGC_ERR_OOB);
        if(err == PGC_ERR_SSL) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE: 
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL: 
                                return LAW_ERR_SYS;
                        default: return LAW_ERR_SSL;
                }
        } else {
                return err;
        }
}

static sel_err_t law_htc_read_sys(
        struct pgc_buf *in,
        int socket,
        const size_t nbytes)
{
        const sel_err_t err = pgc_buf_read(in, socket, nbytes);
        SEL_ASSERT(err != PGC_ERR_OOB);
        if(err == PGC_ERR_SYS) 
                return errno == EAGAIN || errno == EWOULDBLOCK ?
                        LAW_ERR_WNTR :
                        LAW_ERR_SYS;
        else 
                return err;
}

sel_err_t law_htc_read_data(struct law_htconn *conn)
{
        struct pgc_buf *in = conn->in;
        const size_t max = pgc_buf_max(in);
        const size_t end = pgc_buf_end(in);
        const size_t pos = pgc_buf_tell(in);

        const size_t block = end - pos;
        const size_t avail = max - block;

        SEL_ASSERT(pos <= end && block <= max);
        
        if(!avail) {
                return LAW_ERR_OOB;
        } else switch(conn->security) {
                case LAW_HTC_UNSECURED:
                        return law_htc_read_sys(in, conn->socket, avail);
                case LAW_HTC_SSL: 
                        return law_htc_read_SSL(in, conn->ssl, avail);
                default: 
                        return SEL_HALT();
        }
}

sel_err_t law_htc_ensure_input(struct law_htconn *conn, const size_t nbytes)
{
        struct pgc_buf *in = conn->in;
        const size_t offset = pgc_buf_tell(in);
        size_t end = pgc_buf_end(in);
        SEL_ASSERT(offset <= end);
        if((end - offset) >= nbytes) 
                return LAW_ERR_OK;
        SEL_TRY_QUIETLY(law_htc_read_data(conn));
        end = pgc_buf_end(in);
        SEL_ASSERT(offset <= end);
        return (end - offset) >= nbytes ? 
                LAW_ERR_OK : 
                LAW_ERR_WNTR;
}

static sel_err_t law_htc_write_sys(
        struct pgc_buf *out,
        int socket,
        const size_t nbytes)
{
        sel_err_t err = pgc_buf_write(out, socket, nbytes);
        SEL_ASSERT(err != PGC_ERR_OOB);
        if(err == PGC_ERR_SYS) 
                return errno == EAGAIN || errno == EWOULDBLOCK ?
                        LAW_ERR_WNTW : 
                        LAW_ERR_SYS;
        else 
                return err;
}

static sel_err_t law_htc_write_SSL(
        struct pgc_buf *out,
        SSL *ssl,
        const size_t nbytes)
{
        int ssl_err;
        ERR_clear_error();
        const sel_err_t err = pgc_buf_swrite(out, ssl, nbytes, &ssl_err);
        SEL_ASSERT(err != PGC_ERR_OOB);
        if(err == PGC_ERR_SSL) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE: 
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL: 
                                return LAW_ERR_SYS;
                        default: 
                                return LAW_ERR_SSL;
                }
        } else {
                return err;
        }
}

sel_err_t law_htc_write_data(struct law_htconn *conn)
{
        struct pgc_buf *out = conn->out;
        const size_t base = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);
        const size_t block = end - base;
        SEL_ASSERT(base <= end);
        if(!block) {
                return LAW_ERR_OK;
        } else switch(conn->security) {
                case LAW_HTC_SSL:
                        return law_htc_write_SSL(out, conn->ssl, block);
                case LAW_HTC_UNSECURED:
                        return law_htc_write_sys(out, conn->socket, block);
                default: 
                        return SEL_HALT();
        }
}

sel_err_t law_htc_ensure_output(struct law_htconn *conn, const size_t nbytes)
{
        struct pgc_buf *out = conn->out;
        size_t offset = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);
        const size_t max = pgc_buf_max(out);
        SEL_ASSERT(offset <= end);
        size_t unwritten = end - offset;
        SEL_ASSERT(unwritten <= max);
        if((max - unwritten) >= nbytes)
                return LAW_ERR_OK;
        const sel_err_t err = law_htc_write_data(conn);
        if(err != LAW_ERR_OK && err != LAW_ERR_WNTW && err != LAW_ERR_WNTR)
                return err;
        offset = pgc_buf_tell(out);
        SEL_ASSERT(offset <= end);
        unwritten = end - offset;
        SEL_ASSERT(unwritten <= max);
        if((max - unwritten) >= nbytes)
                return LAW_ERR_OK;
        else 
                return LAW_ERR_WNTW;
}

sel_err_t law_htc_flush(struct law_htconn *conn)
{
        struct pgc_buf *out = conn->out;
        size_t offset = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);
        SEL_ASSERT(offset <= end);
        if((end - offset) == 0) 
                return LAW_ERR_OK;
        SEL_TRY_QUIETLY(law_htc_write_data(conn));
        offset = pgc_buf_tell(out);
        return (end - offset) == 0 ? 
                LAW_ERR_OK : 
                LAW_ERR_WNTW;
}

sel_err_t law_htc_read_head(
        struct law_htconn *conn,
        const struct pgc_par *parser,
        struct pgc_stk *stack,
        struct pgc_stk *heap,
        struct pgc_ast_lst **head)
{
        static const char *CRLF2 = "\r\n\r\n";
        static const size_t CRLF2_LEN = 4;

        struct pgc_buf *in = conn->in;
        const size_t base = pgc_buf_tell(in);
        const size_t max = pgc_buf_max(in);

        const sel_err_t read_err = law_htc_read_data(conn); 

        sel_err_t err = pgc_buf_scan(in, (void*)CRLF2, CRLF2_LEN);

        if(err != PGC_ERR_OK) {
                const size_t end = pgc_buf_end(in);
                const size_t block = end - base;
                err = pgc_buf_seek(in, base);
                SEL_ASSERT(err == PGC_ERR_OK && base <= end && block <= max);
                if(block == max) {
                        return LAW_ERR_OOB;
                } else if(read_err == LAW_ERR_OK) {
                        return LAW_ERR_WNTR;
                } else {
                        return read_err;
                }   
        }
        
        const size_t head_end = pgc_buf_tell(in);
        
        err = pgc_buf_seek(in, base);

        SEL_ASSERT(err == PGC_ERR_OK && base <= head_end);

        struct pgc_buf lens; 
        pgc_buf_lens(&lens, in, head_end - base);

        err = pgc_lang_parse_ex(parser, stack, &lens, heap, head);
        if(err != PGC_ERR_OK) 
                return LAW_ERR_SYN;
        SEL_ASSERT(pgc_buf_seek(in, head_end) == PGC_ERR_OK);
        return LAW_ERR_OK;
}

sel_err_t law_htc_ssl_accept(struct law_htconn *conn, SSL_CTX *ssl_ctx)
{
        ERR_clear_error();
        SSL *ssl = SSL_new(ssl_ctx);
        if(!ssl) return LAW_ERR_SSL;

        ERR_clear_error();
        int ssl_err = SSL_set_fd(ssl, conn->socket);
        if(ssl_err == 0) {
                SSL_free(ssl);
                return LAW_ERR_SSL;
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
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE: 
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL:
                                SSL_free(ssl);
                                return LAW_ERR_SYS;
                        default:
                                SSL_free(ssl);
                                return LAW_ERR_SSL;
                }
        } else {
                return LAW_ERR_OK;
        }
}

sel_err_t law_htc_ssl_shutdown(struct law_htconn *conn)
{
        SSL *ssl = conn->ssl;

        ERR_clear_error();
        int ssl_err = SSL_shutdown(ssl);

        if(ssl_err == 0) {
                /* Notify sent, waiting on client response. */
                return LAW_ERR_WNTR;
        } else if(ssl_err < 0) {
                ssl_err = SSL_get_error(ssl, ssl_err);
                switch(ssl_err) {
                        case SSL_ERROR_WANT_READ: 
                                return LAW_ERR_WNTR;
                        case SSL_ERROR_WANT_WRITE: 
                                return LAW_ERR_WNTW;
                        case SSL_ERROR_SYSCALL: 
                                return LAW_ERR_SYS;
                        default: 
                                return LAW_ERR_SSL;
                }
        } else {
                return LAW_ERR_OK; 
        }
}

sel_err_t law_htc_ssl_free(struct law_htconn *conn)
{
        SSL_free(conn->ssl);
        return LAW_ERR_OK;
}
