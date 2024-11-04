
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

void law_log_error(
        FILE *errors,
        const char *action,
        const char *message)
{
        struct law_time_dtb buf;
        char *datetime = law_time_datetime(&buf);
        pid_t pid = getpid();
        SEL_TEST(fprintf(errors, 
                "[%s] %i %s \"%s\"\r\n", 
                datetime,
                pid,
                action,
                message) > 0);
}

sel_err_t law_log_error_ip(
        FILE *errors,
        const int socket,
        const char *action,
        const char *message)
{
        struct law_log_ip_buf ipbuf;
        if(!law_log_ntop(socket, &ipbuf)) {
                law_log_error(errors, "law_log_ntop", strerror(errno));
                return SEL_FREPORT(errors, SEL_ERR_SYS);
        }
        struct law_time_dtb buf;
        char *datetime = law_time_datetime(&buf);
        pid_t pid = getpid();
        SEL_TEST(fprintf(errors, 
               "[%s] [%s] %i %s \"%s\"\r\n", 
                datetime,
                ipbuf.bytes,
                pid,
                action,
                message) > 0);
        return LAW_ERR_OK;
}

