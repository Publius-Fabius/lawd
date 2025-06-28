#ifndef LAWD_BUFFER_H
#define LAWD_BUFFER_H

#include "pgenc/buffer.h"
#include <openssl/ssl.h>

/**
 * Read from the file descriptor.
 * 
 * LAW_ERR_EOF
 * LAW_ERR_WANTR
 * LAW_ERR_SYS
 */
intptr_t law_buf_read(struct pgc_buf *buf, int fd, const size_t len);

/**
 * Write to the buffer.
 * 
 * LAW_ERR_SYS
 * LAW_ERR_WANTW
 */
intptr_t law_buf_write(struct pgc_buf *buf, int fd, const size_t len);

intptr_t law_buf_read_SSL(struct pgc_buf *buf, SSL *ssl, const size_t len);

intptr_t law_buf_write_SSL(struct pgc_buf *buf, SSL *ssl, const size_t len);

#endif