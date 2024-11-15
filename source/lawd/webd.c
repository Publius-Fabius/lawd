#define _POSIX_C_SOURCE 200112L

#include "lawd/webd.h"
#include "lawd/log.h"
#include "lawd/time.h"
#include <errno.h>
#include <openssl/err.h>
#include <stdio.h>

struct law_webd {
        struct law_wd_cfg cfg;
};

struct law_wd_cfg law_wd_sanity()
{
        static struct law_wd_cfg cfg;
        memset(&cfg, 0, sizeof(struct law_wd_cfg));
        cfg.read_head_timeout           = 5000;
        cfg.ssl_shutdown_timeout        = 5000;
        cfg.data                        = (struct law_data){.u = {.u64 = 0}};
        cfg.access                      = stdout;
        return cfg;
}

struct law_webd *law_wd_create(struct law_wd_cfg *cfg)
{
        struct law_webd *webd = calloc(1, sizeof(struct law_webd));
        webd->cfg = *cfg;
        return webd;
}

void law_wd_destroy(struct law_webd *webd) 
{
        free(webd);
}

FILE *law_wd_access(struct law_webd *webd)
{
        return webd->cfg.access;
}

sel_err_t law_wd_log_error(
        FILE *stream,
        const int socket,
        const char *action,
        sel_err_t error)
{
        const char *str = NULL;
        switch(error) {
                case LAW_ERR_SYS:
                        str = strerror(errno);
                        law_log_error_ip(stream, socket, action, str);
                        return error;
                case LAW_ERR_SSL:
                        str = ERR_reason_error_string(ERR_get_error());
                        law_log_error_ip(stream, socket, action, str);
                        return error;
                default:
                        str = sel_strerror(error);
                        law_log_error_ip(stream, socket, action, str);
                        return error;
        }
}

sel_err_t law_wd_log_access(
        FILE *access,
        const int socket,
        const char *method,
        struct law_uri *target,
        const char *version,
        const int status,
        const size_t content)
{
        struct law_log_ip_buf ipbuf;
        char *ipaddr = law_log_ntop(socket, &ipbuf);
        if(!ipaddr) 
                ipaddr = "NTOP_ERROR";
        struct law_time_dt_buf buf;
        char *datetime = law_time_datetime(&buf);
        (void)flockfile(access);
        SEL_TEST(fprintf(access, 
                "%s - - [%s] \"%s ", 
                ipaddr,
                datetime,
                method) > 0)
        SEL_TEST(law_uri_fprint(access, target) == LAW_ERR_OK); 
        SEL_TEST(fprintf(access, 
                " %s\" %i %zu\r\n", 
                version, 
                status, 
                content) > 0);
        (void)funlockfile(access);
        return LAW_ERR_OK;
}

static sel_err_t law_wd_service_conn(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *req,
        struct law_data data)
{
        struct pgc_buf *out = req->conn.out;
        const int sock = req->conn.socket;
        FILE *errs = law_srv_errors(law_srv_server(worker));
        law_wd_onerror_t onerror = webd->cfg.onerror;
        const size_t begin_content = pgc_buf_tell(out);
        
        struct law_hts_head head;
        memset(&head, 0, sizeof(struct law_hts_head));
        sel_err_t err = law_srv_await2(
                worker, 
                sock, 
                webd->cfg.read_head_timeout,
                (law_srv_io2_t)law_hts_read_head,
                req, 
                &head);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(errs, sock, "law_hts_read_head", err);
                SEL_FREPORT(errs, LAW_ERR_TTL);
        }
        
        int status = 0;
        if(err == LAW_ERR_TTL) {
                status = 408; 
                err = onerror(webd, worker, req, status, data);
        } else if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) { 
                status = 500;
        } else if(err != LAW_ERR_OK) {
                status = 500;
                err = onerror(webd, worker, req, status, data);
        } else {
                err = webd->cfg.handler(webd, worker, req, &head, data);
                if(err == LAW_ERR_OK) {
                        status = 200;
                } else if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) {
                        status = 500;
                } else if(err < 0) {
                        status = 500;
                        err = onerror(webd, worker, req, status, data);
                } else {
                        status = err;
                        err = onerror(webd, worker, req, status, data);
                }
        }

        const size_t end_content = pgc_buf_tell(out);
        SEL_ASSERT(begin_content <= end_content);
        law_wd_log_access(
                webd->cfg.access, 
                sock, 
                head.method, 
                &head.target, 
                head.version, 
                status, 
                end_content - begin_content);
        return err;
}

static void law_wd_accept_ssl(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *req,
        struct law_data data)
{
        if(req->conn.security != LAW_HTC_SSL) {
                SEL_ASSERT(req->conn.security == LAW_HTC_UNSECURED);
                (void)law_wd_service_conn(webd, worker, req, data);
                return;
        }
        
        FILE *errs = law_srv_errors(law_srv_server(worker));
        const int socket = req->conn.socket;
        sel_err_t err = law_htc_ssl_accept(&req->conn, req->ctx->ssl_ctx);
        if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) {
                law_wd_log_error(errs, socket, "law_htc_ssl_accept", err);
                SEL_FREPORT(errs, err);
                return;
        }
        err = law_wd_service_conn(webd, worker, req, data);
        if(err != LAW_ERR_SYS && err != LAW_ERR_SSL) {
                if(law_srv_await1(
                        worker,
                        socket,
                        webd->cfg.ssl_shutdown_timeout,
                        (law_srv_io1_t)law_htc_ssl_shutdown,
                        &req->conn)
                        != LAW_ERR_OK)
                {
                        law_wd_log_error(
                                errs, 
                                socket, 
                                "law_htc_ssl_shutdown", 
                                err);
                        SEL_FREPORT(errs, err);
                }
        } 
        law_htc_ssl_free(&req->conn);
}

static void law_wd_accept_sys(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_hts_req *req,
        struct law_data data)
{
        struct law_htconn *conn = &req->conn;
        const int socket = conn->socket;
        FILE *errs = law_srv_errors(law_srv_server(worker));
        struct law_event ev; 
        memset(&ev, 0, sizeof(struct law_event));
        const sel_err_t err = law_srv_add(worker, socket, &ev);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(errs, socket, "law_srv_add", err);
                SEL_FREPORT(errs, err);
        } else {
                (void)law_wd_accept_ssl(webd, worker, req, data);
                law_srv_del(worker, socket);
        }
        law_htc_close(conn);
}

sel_err_t law_wd_accept(
        struct law_worker *worker,
        struct law_hts_req *req,
        struct law_data data)
{
        struct law_webd *webd = data.u.ptr;
        (void)law_wd_accept_sys(webd, worker, req, webd->cfg.data);
        return LAW_ERR_OK;
}