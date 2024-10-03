
#include "lawd/webd.h"
#include "lawd/log.h"
#include "lawd/time.h"

#include <errno.h>
#include <openssl/err.h>

struct law_webd {
        struct law_wd_cfg cfg;
};

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

static sel_err_t law_wd_log_error(
        FILE *errs,
        const int sock,
        const char *action,
        sel_err_t error)
{
        const char *str;
        switch(error) {
                case LAW_ERR_SYS:
                        str = strerror(errno);
                        law_log_error_ip(errs, sock, "lawd", action, str);
                        return error;
                case LAW_ERR_SSL:
                        str = ERR_reason_error_string(ERR_get_error());
                        law_log_error_ip(errs, sock, "lawd", action, str);
                        return error;
                default:
                        str = sel_strerror(error);
                        law_log_error_ip(errs, sock, "lawd", action, str);
                        return error;
        }
}

static sel_err_t law_wd_io_arg(
        struct law_worker *worker,
        struct law_ht_sreq *request,
        int64_t timeout,
        const char *action,
        sel_err_t (*io)(struct law_ht_sreq*, void*),
        void *arg)
{
        struct law_server *server = law_srv_server(worker);
        FILE *errs = law_srv_errors(server);
        const int sock = law_ht_sreq_socket(request);
        int64_t now = law_time_millis();
        const int64_t wakeup = timeout + now;
        struct law_event ev = { .fd = sock, .flags = 0 };
        for(;; ev.flags = 0) {
                const sel_err_t err = io(request, arg);
                switch(err) {
                        case LAW_ERR_OK: 
                                return LAW_ERR_OK;
                        case LAW_ERR_WNTW: 
                                ev.flags = LAW_SRV_POUT;
                                break;
                        case LAW_ERR_WNTR: 
                                ev.flags = LAW_SRV_PIN;
                                break;
                        default: 
                                law_wd_log_error(errs, sock, action, err);
                                return err;
                }
                now = law_time_millis();
                if(wakeup <= now) {
                        return LAW_ERR_TTL;
                }
                if(law_srv_poll(worker, &ev) != LAW_ERR_OK) {
                        law_wd_log_error(errs, sock, "law_srv_poll", err);
                        return SEL_FREPORT(errs, err);
                }
                if(law_srv_wait(worker, wakeup - now) == LAW_ERR_TTL) {
                        return LAW_ERR_TTL;
                }
        }
        return SEL_ABORT();
}

struct law_wd_io_clos {
        sel_err_t (*callback)(struct law_ht_sreq*);
};

static sel_err_t law_wd_io_dis(struct law_ht_sreq *req, void *st)
{
        struct law_wd_io_clos *closure = st;
        return closure->callback(req);
}

static sel_err_t law_wd_io(
        struct law_worker *wrkr,
        struct law_ht_sreq *req,
        int64_t timeout,
        const char *act,
        sel_err_t (*io)(struct law_ht_sreq*))
{
        struct law_wd_io_clos clos;
        clos.callback = io;
        return law_wd_io_arg(wrkr, req, timeout, act, law_wd_io_dis, &clos);
}

static sel_err_t law_wd_read_head(struct law_ht_sreq *req, void *ptr)
{
        return law_ht_sreq_read_head(req, (struct law_ht_reqhead*)ptr);
}

static sel_err_t law_wd_service_conn(
        struct law_webd *webd,
        struct law_worker *worker,
        struct law_ht_sreq *req,
        struct law_data data)
{
        const int socket = law_ht_sreq_socket(req);
        FILE *errs = law_srv_errors(law_srv_server(worker));
        law_wd_onerror_t onerror = webd->cfg.onerror;
        const size_t begin_content = pgc_buf_tell(law_ht_sreq_out(req));
        struct law_ht_reqhead head;
        sel_err_t err;
        int status;
        err = law_wd_io_arg(
                worker, 
                req, 
                webd->cfg.read_head_timeout,
                "law_wd_read_head", 
                law_wd_read_head,
                &head);
        if(err == LAW_ERR_TTL) {
                SEL_FREPORT(errs, LAW_ERR_TTL);
                status = 408; 
                err = onerror(webd, worker, req, status, data);
        } else if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) { 
                SEL_FREPORT(errs, err);
                status = 500;
        } else if(err != LAW_ERR_OK) {
                SEL_FREPORT(errs, err);
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
        law_log_access(
                webd->cfg.access, 
                socket, 
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
        const int sock = law_ht_sreq_socket(req);
        FILE *errs = law_srv_errors(law_srv_server(worker));
        sel_err_t err;
        if(law_ht_sreq_security(req) != LAW_HT_SSL) {
                law_wd_service_conn(webd, worker, req, data);
                return;
        }
        err = law_ht_sreq_ssl_accept(req);
        if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) {
                law_wd_log_error(errs, sock, "law_ht_sreq_ssl_accept", err);
                SEL_FREPORT(errs, err);
                return;
        }
        err = law_wd_service_conn(webd, worker, req, data);
        if(err != LAW_ERR_SYS && err != LAW_ERR_SSL) {
                if(law_wd_io(
                        worker,
                        req,
                        webd->cfg.ssl_shutdown_timeout,
                        "law_ht_sreq_ssl_shutdown",
                        law_ht_sreq_ssl_shutdown) != LAW_ERR_OK)
                {
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
        sel_err_t err = law_srv_watch(worker, socket);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(errs, socket, "law_srv_watch", err);
                SEL_FREPORT(errs, err);
        } else {
                law_wd_accept_ssl(webd, worker, req, data);
                law_srv_unwatch(worker, socket);
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