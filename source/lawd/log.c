
#include "lawd/log.h"
#include "lawd/time.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

sel_err_t law_log_get_ip(
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

sel_err_t law_log_error(
        FILE *errors,
        const char *package,
        const char *message)
{
        struct law_time_dtb buf;
        char *datetime = law_time_datetime(&buf);
        
        pid_t pid = getpid();

        fprintf(errors, 
                "[%s] [%i] [%s] [%s]\r\n", 
                datetime,
                pid,
                package,
                message);
                
        return LAW_ERR_OK;
}

/* [datetime] [ip-address] [pid] [package] [message] */
sel_err_t law_log_error_ip(
        FILE *errors,
        const int socket,
        const char *package,
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

        SEL_IO(fprintf(errors, 
                "[%s] [%s] [%i] [%s] [%s]\r\n", 
                datetime,
                ip_addr,
                pid,
                package,
                message));
                
        return LAW_ERR_OK;
}

