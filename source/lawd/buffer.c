
#include "lawd/error.h"
#include "lawd/buffer.h"
#include <unistd.h>
#include <sys/types.h>
#include <openssl/err.h>
#include <errno.h>

intptr_t law_buf_read_cb(void *addr, const size_t length, void *state)
{
        int *fd_ptr = state;
        ssize_t result = read(*fd_ptr, addr, length);
        if(result > 0) {
                return result;
        }
        if(result == 0) {
                return LAW_ERR_EOF;
        }
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
                return LAW_ERR_WANTR;
        }
        return LAW_ERR_SYS;
}

intptr_t law_buf_read(struct pgc_buf *buf, int fd, const size_t length)
{
        return pgc_buf_cbread(buf, length, law_buf_read_cb, &fd);
}

intptr_t law_buf_write_cb(void *addr, const size_t length, void *state)
{
        int *fd_ptr = state;
        ssize_t result = write(*fd_ptr, addr, length);
        if(result >= 0) {
                return result;
        }
        if(errno == EWOULDBLOCK || errno == EAGAIN) {
                return LAW_ERR_WANTW;
        }
        return LAW_ERR_SYS;
}

intptr_t law_buf_write(struct pgc_buf *buf, int fd, const size_t length)
{
        return pgc_buf_cbwrite(buf, length, law_buf_write_cb, &fd);
}

intptr_t law_buf_SSL_read_cb(void *addr, const size_t length, void *state)
{
        SSL *ssl = state;
        ERR_clear_error();
        int result = SSL_read(ssl, addr, (int)length);
        if(result > 0) {
                return result;
        }
        switch(SSL_get_error(ssl, result)) {
                case SSL_ERROR_SYSCALL: return LAW_ERR_SYS;
                case SSL_ERROR_WANT_READ: return LAW_ERR_WANTR;
                case SSL_ERROR_WANT_WRITE: return LAW_ERR_WANTW;
                case SSL_ERROR_ZERO_RETURN: return LAW_ERR_EOF;
                default: return LAW_ERR_SSL;
        }
}

intptr_t law_buf_read_SSL(struct pgc_buf *buf, SSL *ssl, const size_t length)
{
        return pgc_buf_cbread(buf, length, law_buf_SSL_read_cb, ssl);
}

intptr_t law_buf_SSL_write_cb(void *addr, const size_t length, void *state)
{
        SSL *ssl = state;
        ERR_clear_error();
        int result = SSL_write(ssl, addr, (int)length);
        if(result > 0) {
                return result;
        }
        switch(SSL_get_error(ssl, result)) {
                case SSL_ERROR_SYSCALL: return LAW_ERR_SYS;
                case SSL_ERROR_WANT_READ: return LAW_ERR_WANTR;
                case SSL_ERROR_WANT_WRITE: return LAW_ERR_WANTW;
                default: return LAW_ERR_SSL;
        }
}

intptr_t law_buf_write_SSL(struct pgc_buf *buf, SSL *ssl, const size_t length)
{
        return pgc_buf_cbwrite(buf, length, law_buf_SSL_write_cb, ssl);
}