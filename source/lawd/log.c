
#include "lawd/log.h"
#include "lawd/time.h"
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

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

sel_err_t law_log_error(
        FILE *stream,
        const char *action,
        sel_err_t error,
        const char *details,
        const char *file,
        const char *func,
        const int line)
{
        struct law_time_dt_buf buf;
        char *datetime = law_time_datetime(&buf);
        pid_t pid = getpid();
        SEL_TEST(fprintf(stream,
                "[%s] %i %s \"%s\"\r\n"
                "    Details: %s\r\n"
                "    Location: %s %s %i\r\n",
                datetime,
                pid,
                action,
                sel_strerror(error),
                details,
                file,
                func,
                line) > 0);
        return error;
}

sel_err_t law_log_net_error(
        FILE *stream,
        const int socket,
        const char *action,
        sel_err_t error,
        const char *details,
        const char *file,
        const char *func,
        const int line)
{
        struct law_log_ip_buf ipbuf;
        char *ipaddr = law_log_ntop(socket, &ipbuf);
        if(!ipaddr) ipaddr = "NTOP_ERROR";
        struct law_time_dt_buf buf;
        char *datetime = law_time_datetime(&buf);
        const pid_t pid = getpid();
        SEL_TEST(fprintf(stream,
                "[%s] [%s] %i %s \"%s\"\r\n"
                "    Details: %s\r\n"
                "    Location: %s %s %i\r\n",
                datetime,
                ipaddr,
                pid,
                action,
                sel_strerror(error),
                details,
                file,
                func,
                line) > 0);
        return error;
}
