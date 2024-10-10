
#include "lawd/webd.h"
#include "lawd/log.h"
#include "lawd/time.h"

#include <errno.h>
#include <openssl/err.h>

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
        struct pgc_buf *in = law_ht_sreq_in(req);
        size_t offset = pgc_buf_tell(in);
        const size_t end = pgc_buf_end(in);
        SEL_ASSERT(offset <= end);
        if(end - offset == 0) {
                return LAW_ERR_OK;
        }
        SEL_TRY_QUIETLY(law_ht_sreq_write_data(req));
        offset = pgc_buf_tell(in);
        if(end - offset == 0) {
                return LAW_ERR_OK;
        }
        return LAW_ERR_OK;
}

sel_err_t law_wd_log_error(
        struct law_worker *worker,
        struct law_ht_sreq *request,
        const char *action,
        sel_err_t error)
{
        struct law_server *server = law_srv_server(worker);
        FILE *errs = law_srv_errors(server);
        const int socket = law_ht_sreq_socket(request);
        const char *str = NULL;
        switch(error) {
                case LAW_ERR_SYS:
                        str = strerror(errno);
                        law_log_error_ip(errs, socket, action, str);
                        return error;
                case LAW_ERR_SSL:
                        str = ERR_reason_error_string(ERR_get_error());
                        law_log_error_ip(errs, socket, action, str);
                        return error;
                default:
                        str = sel_strerror(error);
                        law_log_error_ip(errs, socket, action, str);
                        return error;
        }
}

sel_err_t law_wd_run_io_arg(
        struct law_worker *wrkr,
        struct law_ht_sreq *req,
        int64_t timeout,
        sel_err_t (*io)(struct law_ht_sreq*, void*),
        void *arg)
{
        const int sock = law_ht_sreq_socket(req);
        int64_t now = law_time_millis();
        const int64_t wakeup = timeout + now;
        struct law_event ev = { .fd = sock, .events = 0 };
        for(;;) {
                const sel_err_t err = io(req, arg);
                switch(err) {
                        case LAW_ERR_OK: 
                                return LAW_ERR_OK;
                        case LAW_ERR_WNTW: 
                                ev.events = LAW_SRV_POUT;
                                break;
                        case LAW_ERR_WNTR: 
                                ev.events = LAW_SRV_PIN;
                                break;
                        default: 
                                return err;
                }
                now = law_time_millis();
                if(wakeup <= now) {
                        return LAW_ERR_TTL;
                }
                SEL_TRY_QUIETLY(law_srv_poll(wrkr, wakeup - now, 1, &ev));
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

sel_err_t law_wd_run_io(
        struct law_worker *wrkr,
        struct law_ht_sreq *req,
        int64_t timeout,
        sel_err_t (*io)(struct law_ht_sreq*))
{
        struct law_wd_io_clos clos;
        clos.callback = io;
        return law_wd_run_io_arg(wrkr, req, timeout, law_wd_io_dis, &clos);
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
        const int socket = law_ht_sreq_socket(req);
        FILE *errs = law_srv_errors(law_srv_server(worker));
        law_wd_onerror_t onerror = webd->cfg.onerror;
        const size_t begin_content = pgc_buf_tell(law_ht_sreq_out(req));
        struct law_ht_req_head head;
        sel_err_t err;
        int status;
        err = law_wd_run_io_arg(
                worker, 
                req, 
                webd->cfg.read_head_timeout,
                law_wd_read_head,
                &head);
        if(err != LAW_ERR_OK) {
                law_wd_log_error(worker, req, "law_wd_read_head", err);
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
        FILE *errs = law_srv_errors(law_srv_server(worker));
        sel_err_t err;
        if(law_ht_sreq_security(req) != LAW_HT_SSL) {
                law_wd_service_conn(webd, worker, req, data);
                return;
        }
        err = law_ht_sreq_ssl_accept(req);
        if(err == LAW_ERR_SYS || err == LAW_ERR_SSL) {
                law_wd_log_error(worker, req, "law_ht_sreq_ssl_accept", err);
                SEL_FREPORT(errs, err);
                return;
        }
        err = law_wd_service_conn(webd, worker, req, data);
        if(err != LAW_ERR_SYS && err != LAW_ERR_SSL) {
                if(law_wd_run_io(
                        worker,
                        req,
                        webd->cfg.ssl_shutdown_timeout,
                        law_ht_sreq_ssl_shutdown) != LAW_ERR_OK)
                {
                        law_wd_log_error(
                                worker, req, "law_ht_sreq_ssl_shutdown", err);
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
                law_wd_log_error(worker, req, "law_srv_watch", err);
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