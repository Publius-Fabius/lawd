#ifndef LAWD_WEBSOCK_H
#define LAWD_WEBSOCK_H

#include "lawd/error.h"
#include "lawd/http/server.h"
#include <stdint.h>

/** Maximum WebSocket Accept Length */
#define LAW_WS_ACCEPT_BUF_LEN 29

/** Buffer for Sec-WebSocket-Accept */
struct law_ws_accept_buf {
        uint8_t bytes[LAW_WS_ACCEPT_BUF_LEN];
};

/** WebSocket Masking Key Length */
#define LAW_WS_MASKING_KEY_LEN 4

/** WebSocket Masking Key */
struct law_ws_masking_key {
        uint8_t bytes[LAW_WS_MASKING_KEY_LEN];
};

/** XOR Encryption Context */
struct law_ws_xor {
        struct law_ws_masking_key masking_key;
        size_t offset;
};

/** Web Socket Frame Header */
struct law_ws_head {
        uint8_t fin, opcode;
        struct law_ws_masking_key masking_key;
        uint64_t payload_length;
};

/**
 * Generate the accept string by appending the magic string, performing an 
 * SHA1 hash, and then base64 decoding.
 */
const char *law_ws_gen_accept(
        struct law_ws_accept_buf *buffer,
        const char *key);

/** Perform WebSocket handshake. (non-blocking) */
sel_err_t law_ws_accept(
        struct law_worker *worker,
        struct law_hts_req *request,
        struct law_hts_head *head);

/**
 * Read the start of the frame head.  The payload_length will determine what 
 * happens next.
 * <= 125 -> small frame head
 * == 126 -> medium frame head - 16bit
 * == 127 -> large frame head - 64bit
 */
sel_err_t law_ws_read_head_start(
        struct law_htconn *conn,
        struct law_ws_head *head);

/** Finish reading a small frame head. */
sel_err_t law_ws_read_head_small(
        struct law_htconn *conn,
        struct law_ws_head *head);

/** Finish reading a medium sized frame head. */
sel_err_t law_ws_read_head_medium(
        struct law_htconn *conn,
        struct law_ws_head *head);

/** Finish reading large frame head. */
sel_err_t law_ws_read_head_large(
        struct law_htconn *conn,
        struct law_ws_head *head);

/** Initialize an XOR cipher. */
struct law_ws_xor *law_ws_xor_init(
        struct law_ws_xor *xor,
        struct law_ws_masking_key *key);

/** Generate an XOR cipher from a secure random. */
struct law_ws_xor *law_ws_xor_gen(struct law_ws_xor *cipher);

/** Apply XOR encrytion to the buffer. */
void *law_ws_xor_apply(struct law_ws_xor *xor, void *buf, const size_t len);

/** Write a WebSocket head to the output buffer. */
sel_err_t law_ws_write_head(struct law_htconn *con, struct law_ws_head *head);

#endif