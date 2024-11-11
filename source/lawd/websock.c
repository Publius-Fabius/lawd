
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
        EVP_EncodeBlock(buf->bytes, hash, LAW_WS_ACCEPT_BUF_LEN);
        return (const char*)buf->bytes;
}
 
sel_err_t law_ws_accept(
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_ht_req_head *head)
{
        FILE *errs = law_srv_errors(law_srv_server(worker));
        struct law_ht_hdrs *hdrs = head->headers;

        if(!law_ht_hdrs_get(hdrs, "Host")) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Host\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(!law_ht_hdrs_get(hdrs, "Origin")) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Origin\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        }

        const char *upgrade = law_ht_hdrs_get(hdrs, "Upgrade");
        if(!upgrade) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Upgrade\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(strcmp(upgrade, "websocket")) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Upgrade\" != \"websocket\"");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        }

        const char *connection = law_ht_hdrs_get(hdrs, "Connection");
        if(!connection) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Connection\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(strcmp(connection, "Upgrade")) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Connection\" != \"Upgrade\"");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        }

        const char *version = law_ht_hdrs_get(hdrs, "Sec-WebSocket-Version");
        if(!version) {
               law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Sec-WebSocket-Version\" Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS); 
        } else if(strcmp(version, "13")) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Header \"Sec-WebSocket-Version\" != \"13\"");
                return SEL_FREPORT(errs, LAW_ERR_WS); 
        }

        const char *key = law_ht_hdrs_get(hdrs, "Sec-WebSocket-Key");
        if(!key) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Sec-WebSocket-Key Is Missing");
                return SEL_FREPORT(errs, LAW_ERR_WS);
        } else if(strlen(key) > 24) {
                law_wd_log_error(
                        worker, 
                        req, 
                        "law_ws_accept", 
                        "Sec-WebSocket-Key Too Large");
                return SEL_FREPORT(errs, LAW_ERR_WS);
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

sel_err_t law_ws_read_head_start(
        struct law_ht_sreq *request,
        struct law_ws_head *head)
{
        static const size_t BUF_LEN = 2;
        struct pgc_buf *in = law_ht_sreq_in(request);
        uint8_t buf[BUF_LEN];
        SEL_TRY_QUIETLY(law_wd_ensure(request, BUF_LEN));
        SEL_TRY_QUIETLY(pgc_buf_get(in, buf, BUF_LEN)); 
        head->fin = buf[0] & 1;
        head->opcode = buf[0] >> 4;
        head->mask = 1 & buf[1];
        if(!head->mask) {
                return LAW_ERR_WS;
        }
        head->payload_length = buf[1] >> 1;
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_small(
        struct law_ht_sreq *request,
        struct law_ws_head *head)
{
        struct pgc_buf *in = law_ht_sreq_in(request);
        struct law_ws_masking_key *key = &head->masking_key;
        SEL_TRY_QUIETLY(law_wd_ensure(request, LAW_WS_MASKING_KEY_LEN));
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, LAW_WS_MASKING_KEY_LEN)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_medium(
        struct law_ht_sreq *request,
        struct law_ws_head *head)
{
        /* 2 bytes for payload length, 4 bytes for the masking key */
        static const size_t WANT_BYTES = 6;
        struct pgc_buf *in = law_ht_sreq_in(request);
        struct law_ws_masking_key *key = &head->masking_key;
        uint16_t length;
        SEL_TRY_QUIETLY(law_wd_ensure(request, WANT_BYTES));
        SEL_TRY_QUIETLY(pgc_buf_get(in, &length, sizeof(uint16_t)));
        head->payload_length = ntohs(length);
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, LAW_WS_MASKING_KEY_LEN)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head_large(
        struct law_ht_sreq *request,
        struct law_ws_head *head)
{
        /* 8 bytes for payload length, 4 bytes for the masking key */
        static const size_t WANT_BYTES = 12;
        struct pgc_buf *in = law_ht_sreq_in(request);
        struct law_ws_masking_key *key = &head->masking_key;
        uint64_t length;
        SEL_TRY_QUIETLY(law_wd_ensure(request, WANT_BYTES));
        SEL_TRY_QUIETLY(pgc_buf_get(in, &length, sizeof(uint64_t)));
        length = be64toh(length);
        SEL_TRY_QUIETLY(pgc_buf_get(in, key->bytes, LAW_WS_MASKING_KEY_LEN)); 
        return LAW_ERR_OK;
}

sel_err_t law_ws_read_head(
        struct law_worker *worker,
        struct law_ht_sreq *request,
        const int64_t timeout,
        struct law_ws_head *head)
{
        FILE *errs = law_srv_errors(law_srv_server(worker));
        const char *action;
        law_wd_io_arg_t call;

        sel_err_t err = law_wd_io_arg(
                worker, 
                request, 
                timeout, 
                (law_wd_io_arg_t)law_ws_read_head_start,
                head);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(
                        worker, 
                        request, 
                        "law_ws_read_frame_head_start", 
                        err);
                return SEL_FREPORT(errs, err);
        } 

        if(head->payload_length <= 125) {
                call = (law_wd_io_arg_t)law_ws_read_head_small;
                action = "law_ws_read_frame_head_small";
        } else if(head->payload_length == 126) {
                call = (law_wd_io_arg_t)law_ws_read_head_medium;
                action = "law_ws_read_frame_head_medium";
        } else if(head->payload_length == 127) {
                call = (law_wd_io_arg_t)law_ws_read_head_large;
                action = "law_ws_read_frame_head_large";
        }

        err = law_wd_io_arg(worker, request, timeout, call, head);

        if(err != LAW_ERR_OK) {
                law_wd_log_error(worker, request, action, err);
                return SEL_FREPORT(errs, err);
        } 

        return LAW_ERR_OK;
}