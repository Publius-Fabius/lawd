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

struct law_wd_cfg *law_wd_cfg_sanity()
{
        static struct law_wd_cfg cfg;
        cfg.read_head_timeout           = 5000;
        cfg.ssl_shutdown_timeout        = 5000;
        cfg.data                        = (struct law_data){.u = {.u64 = 0}};
        cfg.handler                     = NULL;
        cfg.onerror                     = NULL;
        cfg.access                      = stdout;
        return &cfg;
}

struct law_webd *law_wd_create(struct law_wd_cfg *cfg)
{
        struct law_webd *webd = malloc(sizeof(struct law_webd));
        webd->cfg = *cfg;
        return webd;
}

void law_wd_destroy(struct law_webd *webd) {
        free(webd);
}

FILE *law_wd_access(struct law_webd *webd)
{
        return webd->cfg.access;
}

sel_err_t law_wd_ensure(struct law_ht_sreq *req, const size_t nbytes)
{
        struct pgc_buf *in = law_ht_sreq_in(req);
        const size_t offset = pgc_buf_tell(in);
        size_t end = pgc_buf_end(in);
        SEL_ASSERT(offset <= end);
        if(end - offset >= nbytes) {
                return LAW_ERR_OK;
        }
        SEL_TRY_QUIETLY(law_ht_sreq_read_data(req));
        end = pgc_buf_end(in);
        SEL_ASSERT(offset <= end);
        if(end - offset >= nbytes) {
                return LAW_ERR_OK;
        }
        return LAW_ERR_WNTR;
}

sel_err_t law_wd_flush(struct law_ht_sreq *req)
{
        struct pgc_buf *out = law_ht_sreq_out(req);
        size_t offset = pgc_buf_tell(out);
        const size_t end = pgc_buf_end(out);
        SEL_ASSERT(offset <= end);
        if(end - offset == 0) {
                return LAW_ERR_OK;
        }
        SEL_TRY_QUIETLY(law_ht_sreq_write_data(req));
        offset = pgc_buf_tell(out);
        if(end - offset == 0) {
                return LAW_ERR_OK;
        } else {
                return LAW_ERR_WNTW;
        }
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

/**
 * CLF := ip-address - - [datetime] "request-line" status-code bytes-sent
 */
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
        if(!ipaddr) ipaddr = "NTOP_ERROR";
        struct law_time_dtb buf;
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

sel_err_t law_wd_run_io1(
        struct law_worker *wrk,
        struct law_ht_sreq *req,
        int64_t timeout,
        sel_err_t (*io)(struct law_ht_sreq*, void*),
        void *arg)
{
        const int sock = law_ht_sreq_socket(req);
        int64_t now = law_time_millis();
        const int64_t wakeup = timeout + now;
        struct law_event ev; memset(&ev, 0, sizeof(struct law_event));
        for(int I = 0; I < 512; ++I) {
                sel_err_t err = io(req, arg);
                int event;
                switch(err) {
                        case LAW_ERR_OK: 
                                return LAW_ERR_OK;
                        case LAW_ERR_WNTW: 
                                event = LAW_SRV_POLLOUT;
                                break;
                        case LAW_ERR_WNTR: 
                                event = LAW_SRV_POLLIN;
                                break;
                        default: 
                                return err;
                }
                now = law_time_millis();
                if(wakeup <= now) 
                        return LAW_ERR_TTL;
                else if(event != ev.events)
                        SEL_TRY_QUIETLY(law_srv_mod(wrk, sock, &ev));
                if((err = law_srv_wait(wrk, wakeup - now)) < 0)
                        return err;                
        }
        return LAW_ERR_TTL;
}

struct law_wd_io_clos {
        sel_err_t (*callback)(struct law_ht_sreq*);
};

static sel_err_t law_wd_io_dis(struct law_ht_sreq *req, void *st)
{
        struct law_wd_io_clos *closure = st;
        return closure->callback(req);
}

sel_err_t law_wd_run_io0(
        struct law_worker *wrkr,
        struct law_ht_sreq *req,
        int64_t timeout,
        sel_err_t (*io)(struct law_ht_sreq*))
{
        struct law_wd_io_clos clos;
        clos.callback = io;
        return law_wd_run_io1(wrkr, req, timeout, law_wd_io_dis, &clos);
}

static sel_err_t law_wd_read_head(struct law_ht_sreq *req, void *ptr)
{
        return law_ht_sreq_read_head(req, (struct law_ht_req_head*)ptr);
}

static sel_err_t law_wd_service_conn(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_data data)
{
        const int sock = law_ht_sreq_socket(req);
        FILE *errs = law_srv_errors(law_srv_server(worker));
        law_wd_onerror_t onerror = webd->cfg.onerror;
        const size_t begin_content = pgc_buf_tell(law_ht_sreq_out(req));
        struct law_ht_req_head head;
        sel_err_t err;
        int status;
        err = law_wd_run_io1(
                worker, 
                req, 
                webd->cfg.read_head_timeout,
                law_wd_read_head,
                &head);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(errs, sock, "law_wd_read_head", err);
                SEL_FREPORT(errs, LAW_ERR_TTL);
        }

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
        law_wd_log_access(
                webd->cfg.access, 
                sock, 
                head.method, 
                &head.target, 
                head.version, 
                status, 
                pgc_buf_tell(law_ht_sreq_out(req)) - begin_content);
        return err;
}

static void law_wd_accept_ssl(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_data data)
{
        FILE *errs = law_srv_errors(law_srv_server(worker));
        const int socket = law_ht_sreq_socket(req);
        sel_err_t err;
        if(law_ht_sreq_security(req) != LAW_HT_SSL) {
                law_wd_service_conn(webd, worker, req, data);
                return;
        }
        err = law_ht_sreq_ssl_accept(req);
        if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) {
                law_wd_log_error(errs, socket, "law_ht_sreq_ssl_accept", err);
                SEL_FREPORT(errs, err);
                return;
        }
        err = law_wd_service_conn(webd, worker, req, data);
        if(err != LAW_ERR_SYS && err != LAW_ERR_SSL) {
                if(law_wd_run_io0(
                        worker,
                        req,
                        webd->cfg.ssl_shutdown_timeout,
                        law_ht_sreq_ssl_shutdown) != LAW_ERR_OK)
                {
                        law_wd_log_error(
                                errs, 
                                socket, 
                                "law_ht_sreq_ssl_shutdown", 
                                err);
                        SEL_FREPORT(errs, err);
                }
        } 
        law_ht_sreq_ssl_free(req);
}

static void law_wd_accept_sys(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_data data)
{
        const int socket = law_ht_sreq_socket(req);
        FILE *errs = law_srv_errors(law_srv_server(worker));
        struct law_event ev; memset(&ev, 0, sizeof(struct law_event));
        sel_err_t err = law_srv_add(worker, socket, &ev);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(errs, socket, "law_srv_add", err);
                SEL_FREPORT(errs, err);
        } else {
                law_wd_accept_ssl(webd, worker, req, data);
                law_srv_del(worker, socket);
        }
        law_ht_sreq_close(req);
}

sel_err_t law_wd_accept(
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_data data)
{
        struct law_webd *webd = data.u.ptr;
        law_wd_accept_sys(webd, worker, req, webd->cfg.data);
        return LAW_ERR_OK;
}