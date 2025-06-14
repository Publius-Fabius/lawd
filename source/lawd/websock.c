
/** For be64toh, htobe64. */
#define _DEFAULT_SOURCE

#include "lawd/websock.h"
#include "lawd/webd.h"
#include "lawd/log.h"
#include <endian.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <arpa/inet.h>

/** WebSocket Magic String */
#define LAW_WS_MAGIC_STR "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

/** WebSocket Magic String Length */
#define LAW_WS_MAGIC_STR_LEN 36

const char *law_ws_gen_accept(
        struct law_ws_accept_buf *buf,
        const char *key)
{
        uint8_t hash[EVP_MAX_MD_SIZE];
        memset(hash, 0, EVP_MAX_MD_SIZE);
        EVP_MD_CTX *sha_ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(sha_ctx, EVP_sha1(), NULL);
        EVP_DigestUpdate(sha_ctx, key, strlen(key));
        EVP_DigestUpdate(sha_ctx, LAW_WS_MAGIC_STR, LAW_WS_MAGIC_STR_LEN);
        unsigned int digest_len = 0;
        EVP_DigestFinal_ex(sha_ctx, hash, &digest_len);
        SEL_ASSERT(digest_len == 20);
        EVP_EncodeBlock(buf->bytes, hash, (int)digest_len);
        return (const char*)buf->bytes;
}
 
sel_err_t law_ws_accept(
        struct law_worker *worker,
        struct law_hts_req *req,
        struct law_hts_head *head)
{
        FILE *errs = law_get_errors(law_get_server(worker));
        struct law_hthdrs *hdrs = head->headers;
        const int sock = req->conn.socket;

        if(!law_hth_get(hdrs, "Host")) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Host\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(!law_hth_get(hdrs, "Origin")) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Origin\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        }

        const char *upgrade = law_hth_get(hdrs, "Upgrade");
        if(!upgrade) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Upgrade\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(strcmp(upgrade, "websocket")) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Upgrade\" != \"websocket\"");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        }

        const char *connection = law_hth_get(hdrs, "Connection");
        if(!connection) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Connection\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } 
        
        // else if(strcmp(connection, "Upgrade")) {
        //         law_log_error_ip(
        //                 errs, 
        //                 sock, 
        //                 "law_ws_accept", 
        //                 "Header \"Connection\" != \"Upgrade\"");
        //         return SEL_FREPORT(errs, LAW_ERR_WS);
        // }

        const char *version = law_hth_get(hdrs, "Sec-WebSocket-Version");
        if(!version) {
               law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Sec-WebSocket-Version\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS); 
        } else if(strcmp(version, "13")) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Header \"Sec-WebSocket-Version\" != \"13\"");
                return SEL_FREPORT(errs, LAW_ERR_WS); 
        }

        const char *key = law_hth_get(hdrs, "Sec-WebSocket-Key");
        if(!key) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Sec-WebSocket-Key Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(strlen(key) > 24) {
                law_log_error_ip(
                        errs, 
                        sock, 
                        "law_ws_accept", 
                        "Sec-WebSocket-Key Too Large");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        }

        struct law_ws_accept_buf acc_buf;
        const char *accept = law_ws_gen_accept(&acc_buf, key);
        
        SEL_TRY_QUIETLY(law_hts_set_status(
                req, "HTTP/1.1", 101, "Switching Protocols"));
        SEL_TRY_QUIETLY(law_hts_add_header(
                req, "Upgrade", "websocket"));
        SEL_TRY_QUIETLY(law_hts_add_header(
                req, "Connection", "Upgrade"));
        SEL_TRY_QUIETLY(law_hts_add_header(
                req, "Sec-WebSocket-Accept", accept));
        SEL_TRY_QUIETLY(law_hts_begin_body(req));

        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_start(
        struct law_htconn *conn,
        struct law_ws_head *head)
{
        struct pgc_buf *in = conn->in; 
        uint8_t buf[2];
        SEL_TRY_QUIETLY(law_htc_ensure_input(conn, 2));
        SEL_TRY_QUIETLY(pgc_buf_get(in, buf, 2)); 
        head->fin = buf[0] >> 7;
        head->opcode = buf[0] & 0xF;
        if(!(buf[1] & 0x80)) return LAW_ERR_WS;
        head->payload_length = buf[1] & 0x7F;
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_small(
        struct law_htconn *conn,
        struct law_ws_head *head)
{
        struct pgc_buf *in = conn->in;
        struct law_ws_masking_key *key = &head->masking_key;
        SEL_TRY_QUIETLY(law_htc_ensure_input(conn, 4));
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, 4)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_medium(
        struct law_htconn *conn,
        struct law_ws_head *head)
{
        /* 2 bytes for payload length, 4 bytes for the masking key */
        static const size_t WANT_BYTES = 6;
        struct pgc_buf *in = conn->in;
        struct law_ws_masking_key *key = &head->masking_key;
        uint16_t length;
        SEL_TRY_QUIETLY(law_htc_ensure_input(conn, WANT_BYTES));
        SEL_TRY_QUIETLY(pgc_buf_get(in, &length, sizeof(uint16_t)));
        head->payload_length = ntohs(length);
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, 4)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_large(
        struct law_htconn *conn,
        struct law_ws_head *head)
{
        /* 8 bytes for payload length, 4 bytes for the masking key */
        static const size_t WANT_BYTES = 12;
        struct pgc_buf *in = conn->in;
        struct law_ws_masking_key *key = &head->masking_key;
        uint64_t length;
        SEL_TRY_QUIETLY(law_htc_ensure_input(conn, WANT_BYTES));
        SEL_TRY_QUIETLY(pgc_buf_get(in, &length, sizeof(uint64_t)));
        length = be64toh(length);
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, 4)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head(
        struct law_worker *worker,
        struct law_hts_req *request,
        const int64_t timeout,
        struct law_ws_head *head)
{
        FILE *errs = law_get_errors(law_get_server(worker));
        struct law_htconn *conn = &request->conn;
        const int sock = conn->socket;
        const char *action;
        law_srv_io2_t call;

        sel_err_t err = law_srv_await2(
                worker,
                sock,
                timeout,
                (law_srv_io2_t)law_ws_read_head_start,
                conn,
                head);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(
                        errs, 
                        sock, 
                        "law_ws_read_frame_head_start", 
                        err);
                return SEL_FREPORT(errs, err);
        } 

        if(head->payload_length <= 125) {
                call = (law_srv_io2_t)law_ws_read_head_small;
                action = "law_ws_read_frame_head_small";
        } else if(head->payload_length == 126) {
                call = (law_srv_io2_t)law_ws_read_head_medium;
                action = "law_ws_read_frame_head_medium";
        } else if(head->payload_length == 127) {
                call = (law_srv_io2_t)law_ws_read_head_large;
                action = "law_ws_read_frame_head_large";
        }

        err = law_srv_await2(worker, sock, timeout, call, conn, head);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(errs, sock, action, err);
                return SEL_FREPORT(errs, err);
        } 

        return LAW_ERR_OK;
}

struct law_ws_xor *law_ws_xor_init(
        struct law_ws_xor *xor,
        struct law_ws_masking_key *key)
{
        xor->masking_key = *key;
        xor->offset = 0;
        return xor;
}

struct law_ws_xor *law_ws_xor_gen(struct law_ws_xor *xor)
{
        RAND_bytes(xor->masking_key.bytes, 4);
        xor->offset = 0;
        return xor;
}

void *law_ws_xor_apply(struct law_ws_xor *xor, void *buf, const size_t len)
{
        uint8_t *bs = buf;
        for(size_t n = 0; n < len; ++n) {
                const size_t j = xor->offset++ % 4;
                bs[n] ^= xor->masking_key.bytes[j]; 
        }
        return buf;
}

sel_err_t law_ws_write_head_small(
        struct law_htconn *conn, 
        struct law_ws_head *head)
{
        SEL_ASSERT(head->payload_length <= 125);
        SEL_TRY_QUIETLY(law_htc_ensure_output(conn, 6));
        struct pgc_buf *out = conn->out;
        uint8_t bs[2];
        bs[0] = ((1 & head->fin) << 7) | head->opcode;
        bs[1] = (1 << 7) | (uint8_t)head->payload_length;
        SEL_TRY_QUIETLY(pgc_buf_put(out, bs, 2));
        return pgc_buf_put(out, head->masking_key.bytes, 4);
}

sel_err_t law_ws_write_head_medium(
        struct law_htconn *conn, 
        struct law_ws_head *head)
{
        SEL_ASSERT(head->payload_length <= UINT16_MAX);
        SEL_TRY_QUIETLY(law_htc_ensure_output(conn, 8));
        struct pgc_buf *out = conn->out;
        uint8_t bs[2] = { 0, 0 };
        bs[0] = ((1 & head->fin) << 7) | head->opcode;
        bs[1] = (1 << 7) | 126;
        SEL_TRY_QUIETLY(pgc_buf_put(out, bs, 2));
        uint16_t paylen = htons((uint16_t)head->payload_length);
        SEL_TRY_QUIETLY(pgc_buf_put(out, &paylen, 2));
        return pgc_buf_put(out, head->masking_key.bytes, 4);
}

sel_err_t law_ws_write_head_large(
        struct law_htconn *conn, 
        struct law_ws_head *head)
{
        SEL_TRY_QUIETLY(law_htc_ensure_output(conn, 14));
        struct pgc_buf *out = conn->out;
        uint8_t bs[2] = { 0, 0 };
        bs[0] = ((1 & head->fin) << 7) | head->opcode;
        bs[1] = (1 << 7) | 127;
        SEL_TRY_QUIETLY(pgc_buf_put(out, bs, 2));
        uint64_t paylen = htobe64(head->payload_length);
        SEL_TRY_QUIETLY(pgc_buf_put(out, &paylen, 8));
        return pgc_buf_put(out, head->masking_key.bytes, 4);
}

sel_err_t law_ws_write_head(struct law_htconn *conn, struct law_ws_head *head)
{
        if(head->payload_length <= 125) 
                return law_ws_write_head_small(conn, head);
        else if(head->payload_length <= UINT16_MAX) 
                return law_ws_write_head_medium(conn, head);
        else return law_ws_write_head_large(conn, head);
}
