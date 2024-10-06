
#define _GNU_SOURCE

#include "lawd/log.h"
#include "lawd/time.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

static sel_err_t law_log_get_ip(
        FILE *errs,
        const int sock, 
        char *buf, 
        const socklen_t len)
{
        struct sockaddr_storage addr;
        socklen_t addr_len = sizeof(struct sockaddr_storage);

        SEL_FIO(errs, getsockname(sock, (struct sockaddr*)&addr, &addr_len));

        if(addr.ss_family == AF_INET) {
                struct sockaddr_in *in = (struct sockaddr_in*)&addr;
                if(!inet_ntop(AF_INET, &in->sin_addr, buf, len)) {
                        return SEL_FREPORT(errs, LAW_ERR_SYS);
                }
                return LAW_ERR_OK;
        } else if(addr.ss_family == AF_INET6) {
                struct sockaddr_in6 *in = (struct sockaddr_in6*)&addr;
                if(!inet_ntop(AF_INET6, &in->sin6_addr, buf, len)) {
                        return SEL_FREPORT(errs, LAW_ERR_SYS);
                }
                return LAW_ERR_OK;
        } else {
                return SEL_ABORT();
        }
}

/**
 * CLF := ip-address - - [datetime] "request-line" status-code bytes-sent
 */
sel_err_t law_log_access(
        FILE *access,
        const int socket,
        const char *method,
        struct law_uri *target,
        const char *version,
        const int status,
        const size_t content)
{
        char ip_addr[INET6_ADDRSTRLEN];
        if(law_log_get_ip(
                stderr, 
                socket, 
                ip_addr, 
                INET6_ADDRSTRLEN) != LAW_ERR_OK) {
                fprintf(stderr, "errno:%s\r\n", strerror(errno));
        }

        struct law_time_dtb buf;
        char *datetime = law_time_datetime(&buf);

        flockfile(access);
        fprintf(access, 
                "%s - - [%s] \"%s ", 
                ip_addr,
                datetime,
                method);
        law_uri_fprint(access, target);
        fprintf(access, " %s\" %i %zu\r\n", version, status, content);
        funlockfile(access);

        return LAW_ERR_OK;
}

sel_err_t law_log_error(
        FILE *errors,
        const char *action,
        const char *message)
{
        struct law_time_dtb buf;
        char *datetime = law_time_datetime(&buf);
        
        pid_t pid = getpid();
        pid_t tid = gettid();

        fprintf(errors, 
                "[%s] %i %i %s \"%s\"\r\n", 
                datetime,
                pid,
                tid,
                action,
                message);
                
        return LAW_ERR_OK;
}

sel_err_t law_log_error_ip(
        FILE *errors,
        const int socket,
        const char *action,
        const char *message)
{
        char ip_addr[INET6_ADDRSTRLEN];
        if(law_log_get_ip(
                errors, 
                socket, 
                ip_addr, 
                INET6_ADDRSTRLEN) != LAW_ERR_OK) {
                fprintf(errors, "errno:%s\r\n", strerror(errno));
        }

        struct law_time_dtb buf;
        char *datetime = law_time_datetime(&buf);
        
        pid_t pid = getpid();
        pid_t tid = gettid();

        fprintf(errors, 
               "%s [%s] %i %i %s \"%s\"\r\n", 
                datetime,
                ip_addr,
                pid,
                tid,
                action,
                message);
                
        return LAW_ERR_OK;
}

