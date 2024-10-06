
#define _BSD_SOURCE

#include "lawd/websock.h"
#include "lawd/webd.h"

#include <endian.h>
#include <openssl/evp.h>
#include <arpa/inet.h>

const char *law_ws_gen_accept(
        struct law_ws_accept_buf *buf,
        const char *key)
{
        uint8_t hash[EVP_MAX_MD_SIZE];
        EVP_MD_CTX *sha_ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(sha_ctx, EVP_sha1(), NULL);
        EVP_DigestUpdate(sha_ctx, key, strlen(key));
        EVP_DigestUpdate(sha_ctx, LAW_WS_MAGIC_STR, LAW_WS_MAGIC_STR_LEN);
        EVP_DigestFinal_ex(sha_ctx, hash, NULL);
        EVP_EncodeBlock(buf->bytes, hash, LAW_WS_MAX_ACCEPT);
        return (const char*)buf->bytes;
}
 
sel_err_t law_ws_accept(
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_ht_req_head *head)
{
        struct law_ht_hdrs *hdrs = head->headers;
        const char *key = law_ht_hdrs_get(hdrs, "Sec-WebSocket-Key");
        if(!key) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ht_hdrs_get", 
                        "Sec-WebSocket-Key Is Missing");
                return LAW_ERR_WS;
        }
        struct law_ws_accept_buf acc_buf;
        const char *accept = law_ws_gen_accept(&acc_buf, key);
        SEL_TRY_QUIETLY(law_ht_sreq_set_status(
                req, "HTTP/1.1", 101, "Switching Protocols"));
        SEL_TRY_QUIETLY(law_ht_sreq_add_header(
                req, "Upgrade", "websocket"));
        SEL_TRY_QUIETLY(law_ht_sreq_add_header(
                req, "Connection", "Upgrade"));
        SEL_TRY_QUIETLY(law_ht_sreq_add_header(
                req, "Sec-WebSocket-Accept", accept));
        SEL_TRY_QUIETLY(law_ht_sreq_begin_body(req));
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_frame_head_start(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head)
{
        static const size_t BUF_LEN = 2;
        struct pgc_buf *in = law_ht_sreq_in(request);
        uint8_t buf[BUF_LEN];
        SEL_TRY_QUIETLY(law_wd_ensure_input(request, BUF_LEN));
        SEL_TRY_QUIETLY(pgc_buf_get(in, buf, BUF_LEN)); 
        head->opcode = buf[0] >> 4;
        if(head->opcode > 2) {
                return LAW_ERR_WS;
        }
        head->mask = 1 & buf[1];
        if(!head->mask) {
                return LAW_ERR_WS;
        }
        head->payload_length = buf[1] >> 1;
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_frame_head_start_v(struct law_ht_sreq *r, void *h)
{
        return law_ws_read_frame_head_start(r, (struct law_ws_frame_head*)h);
}

sel_err_t law_ws_read_frame_head_small(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head)
{
        struct pgc_buf *in = law_ht_sreq_in(request);
        struct law_ws_masking_key *key = &head->masking_key;
        SEL_TRY_QUIETLY(law_wd_ensure_input(request, LAW_WS_MASKING_KEY_LEN));
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, LAW_WS_MASKING_KEY_LEN)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_frame_head_small_v(struct law_ht_sreq *r, void *h)
{
        return law_ws_read_frame_head_small(r, (struct law_ws_frame_head*)h);
}

sel_err_t law_ws_read_frame_head_medium(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head)
{
        /* 2 bytes for payload length, 4 bytes for the masking key */
        static const size_t WANT_BYTES = 6;
        struct pgc_buf *in = law_ht_sreq_in(request);
        struct law_ws_masking_key *key = &head->masking_key;
        uint16_t length;
        SEL_TRY_QUIETLY(law_wd_ensure_input(request, WANT_BYTES));
        SEL_TRY_QUIETLY(pgc_buf_get(in, &length, sizeof(uint16_t)));
        head->payload_length = ntohs(length);
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, LAW_WS_MASKING_KEY_LEN)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_frame_head_medium_v(struct law_ht_sreq *r, void *h)
{
        return law_ws_read_frame_head_medium(r, (struct law_ws_frame_head*)h);
}

sel_err_t law_ws_read_frame_head_large(
        struct law_ht_sreq *request,
        struct law_ws_frame_head *head)
{
        /* 8 bytes for payload length, 4 bytes for the masking key */
        static const size_t WANT_BYTES = 12;
        struct pgc_buf *in = law_ht_sreq_in(request);
        struct law_ws_masking_key *key = &head->masking_key;
        uint64_t length;
        SEL_TRY_QUIETLY(law_wd_ensure_input(request, WANT_BYTES));
        SEL_TRY_QUIETLY(pgc_buf_get(in, &length, sizeof(uint64_t)));
        length = be64toh(length);
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, LAW_WS_MASKING_KEY_LEN)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_frame_head_large_v(struct law_ht_sreq *r, void *h)
{
        return law_ws_read_frame_head_large(r, (struct law_ws_frame_head*)h);
}