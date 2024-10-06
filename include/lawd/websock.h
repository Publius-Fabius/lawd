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
#define LAW_WS_MAX_ACCEPT 32

/** Buffer for Sec-WebSocket-Accept */
struct law_ws_accept_buf {
        uint8_t bytes[LAW_WS_MAX_ACCEPT];
};

#define LAW_WS_MASKING_KEY_LEN 4

/** WebSocket Masking Key */
struct law_ws_masking_key {
        uint8_t bytes[LAW_WS_MASKING_KEY_LEN];
};

/** Web Socket Frame Header */
struct law_ws_frame_head {
        uint8_t opcode;
        uint8_t mask;
        struct law_ws_masking_key masking_key;
        size_t payload_length;
};

/**
 * Generate the accept key by appending the magic string, performing an SHA1,
 * and then base64 decoding.
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
 * Read the start of the frame head.  The payload_length will determine
 * what happens next.
 * <= 125 -> small frame head
 * == 126 -> medium frame head
 * == 127 -> large frame head
 */
sel_err_t law_ws_read_frame_head_start(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head);

sel_err_t law_ws_read_frame_head_start_v(
        struct law_ht_sreq *request,
        void *head);

/**
 * Finish reading a small sized frame head.
 */
sel_err_t law_ws_read_frame_head_small(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head);

sel_err_t law_ws_read_frame_head_small_v(
        struct law_ht_sreq *request,
        void *head);

/**
 * Finish reading a medium sized frame head.
 */
sel_err_t law_ws_read_frame_head_medium(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head);

sel_err_t law_ws_read_frame_head_medium_v(
        struct law_ht_sreq *request,
        void *head);

/**
 * Finish reading large sized frame head.
 */
sel_err_t law_ws_read_frame_head_large(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head);

sel_err_t law_ws_read_frame_head_large_v(
        struct law_ht_sreq *request,
        void *head);


#endif