#ifndef LAWD_WEBSOCK_H
#define LAWD_WEBSOCK_H

#include "lawd/error.h"
#include "lawd/http.h"
#include <stdint.h>

/** WebSocket Magic String */
#define LAW_WS_MAGIC_STR "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/** WebSocket Magic String Length */
#define LAW_WS_MAGIC_STR_LEN 36

/** Maximum WebSocket Accept Length */
#define LAW_WS_ACCEPT_BUF_LEN 32

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
        struct law_ws_masking_key *masking_key;
        size_t offset;
};

/** Web Socket Frame Header */
struct law_ws_head {
        uint8_t fin;
        uint8_t opcode;
        uint8_t mask;
        struct law_ws_masking_key masking_key;
        size_t payload_length;
};

/**
 * Generate the accept string by appending the magic string, performing an 
 * SHA1 hash, and then base64 decoding.
 */
const char *law_ws_gen_accept(
        struct law_ws_accept_buf *buffer,
        const char *key);

/**
 * Perform WebSocket handshake.
 */
sel_err_t law_ws_accept(
        struct law_worker *worker,
        struct law_ht_sreq *request,
        struct law_ht_req_head *head);

/**
 * Read the start of the frame head.  The payload_length will determine what 
 * happens next.
 * <= 125 -> small frame head
 * == 126 -> medium frame head
 * == 127 -> large frame head
 */
sel_err_t law_ws_read_head_start(
        struct law_ht_sreq *request,
        struct law_ws_head *head);

/**
 * Finish reading a small frame head.
 */
sel_err_t law_ws_read_head_small(
        struct law_ht_sreq *request,
        struct law_ws_head *head);

/**
 * Finish reading a medium sized frame head.
 */
sel_err_t law_ws_read_head_medium(
        struct law_ht_sreq *request,
        struct law_ws_head *head);

/**
 * Finish reading large frame head.
 */
sel_err_t law_ws_read_head_large(
        struct law_ht_sreq *request,
        struct law_ws_head *head);

/**
 * Initialize an XOR cipher.
 */
struct law_ws_xor *law_ws_xor_init(
        struct law_ws_xor *cipher,
        struct law_ws_masking_key *key);

/**
 * Apply XOR encrytion to the buffer.
 */
sel_err_t law_ws_xor_apply(
        struct law_ws_xor *cipher,
        void *buffer,
        const size_t length);

/**
 * Write a WebSocket head to the output buffer.
 */
sel_err_t law_ws_write_head(
        struct law_ht_sreq *request, 
        struct law_ws_head *head);

#endif