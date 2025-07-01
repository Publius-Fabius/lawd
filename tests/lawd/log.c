
#include "lawd/log.h"
#include <stdlib.h>
#include <errno.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/crypto.h>

void test_log_error()
{
        LAW_LOG_ERR(stdout, LAW_ERR_OOB, "accept");

        errno = EWOULDBLOCK;
        LAW_LOG_ERR(stdout, LAW_ERR_SYS, "accept");

        ERR_clear_error();

        ERR_raise(ERR_LIB_SSL, SSL_R_SSL_HANDSHAKE_FAILURE);
        LAW_ERR_PUSH(LAW_ERR_SSL, "accept");
        LAW_ERR_PUSH(LAW_ERR_SSL, "toady");
        LAW_LOG_ERR(stdout, LAW_ERR_SSL, "accept");
        law_err_print_stack(stdout);
}

int main(int argc, char **args) 
{
        law_err_init();
       // SSL_load_error_strings();

        test_log_error();
}