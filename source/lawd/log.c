
#include "lawd/log.h"
#include "lawd/time.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/err.h>
#include <threads.h>

char *law_log_ntop(
        const int sock, 
        struct law_log_ip_buf *ipbuf)
{
        static const socklen_t BUF_LEN = INET6_ADDRSTRLEN;

        char *buf = ipbuf->bytes;
        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(struct sockaddr_storage);

        if(getsockname(sock, (struct sockaddr*)&addr, &addr_len) == -1)
                return NULL;
        if(addr.ss_family == AF_INET) {
                struct sockaddr_in *in = (struct sockaddr_in*)&addr;
                if(!inet_ntop(AF_INET, &in->sin_addr, buf, BUF_LEN)) 
                        return NULL;
        } else if(addr.ss_family == AF_INET6) {
                struct sockaddr_in6 *in = (struct sockaddr_in6*)&addr;
                if(!inet_ntop(AF_INET6, &in->sin6_addr, buf, BUF_LEN)) 
                        return NULL;
        } else {
                SEL_HALT();
        }

        return buf;
}

static const char *law_log_details(sel_err_t error)
{
        const char *error_str = NULL;

        switch(error) {
                case LAW_ERR_SYS:
                        error_str = strerror(errno);
                        break;
                case LAW_ERR_SSL:
                        error_str = ERR_reason_error_string(ERR_get_error());
                        error_str = error_str ? 
                                error_str :
                                "Call SSL_load_error_strings() first";
                        break;
                default: 
                        error_str = "LAWD built in error";
        }

        return error_str;
}

sel_err_t law_log_err_ex(
        FILE *stream,
        const char *error,
        const char *action,
        const char *details,
        const char *file,
        const char *func,
        const int line)
{
        static const char *DASH = "-";
        static const char *EMPTY = "";

        struct law_time_dt_buf buf;
        char *datetime = law_time_datetime(&buf);

        if(fprintf(stream, 
                "! 0x%zx [%s] %s %s\r\n"
                "%s() : %s : %i\r\n"
                "%s\r\n",
                thrd_current(),
                datetime ? datetime : EMPTY,
                error ? error : DASH,
                action ? action : DASH,
                func ? func : DASH,
                file ? file : DASH,
                line,
                details ? details : EMPTY) == -1)
                return LAW_ERR_SYS;

        return LAW_ERR_OK;
}

sel_err_t law_log_err(
        FILE *stream,
        sel_err_t error,
        const char *action,
        const char *file,
        const char *func,
        const int line)
{
        return law_log_err_ex(
                stream,
                sel_strerror(error),
                action,
                law_log_details(error),
                file,
                func,
                line);
}

sel_err_t law_log_net_err_ex(
        FILE *stream,
        const char *error,
        const char *ip_address,
        const char *user,
        const char *action,
        const char *details,
        const char *file,
        const char *func,
        const int line)
{
        static const char *DASH = "-";
        static const char *EMPTY = "";

        struct law_time_dt_buf buf;
        char *datetime = law_time_datetime(&buf);

        if(fprintf(stream, 
                "! 0x%zx %s %s [%s] %s %s\r\n"
                "%s() : %s : %i\r\n"
                "%s",
                thrd_current(),
                ip_address ? ip_address : DASH,
                user ? user : DASH,
                datetime ? datetime : EMPTY,
                error ? error : DASH,
                action ? action : DASH,
                func ? func : DASH,
                file ? file : DASH,
                line,
                details ? details : EMPTY) == -1)
                return LAW_ERR_SYS;

        return LAW_ERR_OK;
}

sel_err_t law_log_net_err(
        FILE *stream,
        const sel_err_t error,
        const int socket,
        const char *user,
        const char *action,
        const char *file,
        const char *func,
        const int line)
{
        struct law_log_ip_buf ipbuf;
        const char *ip_address = law_log_ntop(socket, &ipbuf);
        return law_log_net_err_ex(
                stream,
                sel_strerror(error),
                ip_address,
                user,
                action,
                law_log_details(error),
                file,
                func,
                line);
}

sel_err_t law_log_access_ex(
        FILE *stream,
        const char *ip_address,
        const char *ident,
        const char *user,
        const char *req_line,
        const int status,
        const size_t content_len)
{
        static const char *DASH = "-";
        static const char *EMPTY = "";

        struct law_time_dt_buf buf;
        char *datetime = law_time_datetime(&buf);

        if(fprintf(stream,
                "%s %s %s [%s] \"%s\" %i %zu\r\n",
                ip_address ? ip_address : DASH,
                ident ? ident : DASH,
                user ? user : DASH, 
                datetime ? datetime : EMPTY,
                req_line ? req_line : EMPTY,
                status,
                content_len) == -1)
                return LAW_ERR_SYS;
        
        return LAW_ERR_OK;
}

sel_err_t law_log_access(
        FILE *stream,
        const int socket,
        const char *ident,
        const char *user,
        const char *req_line,
        const int status_code,
        const size_t content_len)
{
        struct law_log_ip_buf ipbuf;
        const char *ip_address = law_log_ntop(socket, &ipbuf);
        return law_log_access_ex(
                stream,
                ip_address,
                ident,
                user,
                req_line,
                status_code,
                content_len);
}