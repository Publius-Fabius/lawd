
#include "lawd/error.h"

enum pgc_err law_err_pgenc;

int law_err_SSL;

void law_error_init()
{
        sel_init();
        SEL_BIND(LAW_ERR_OK, "All Ok");
        SEL_BIND(LAW_ERR_SYS, "System Call");
        SEL_BIND(LAW_ERR_OOB, "Index Out of Bounds");
        SEL_BIND(LAW_ERR_CMP, "Comparison Failed");
        SEL_BIND(LAW_ERR_ENC, "Bad Encoding");
        SEL_BIND(LAW_ERR_EOF, "End of File Encountered");
        SEL_BIND(LAW_ERR_SYN, "Bad Syntax");
        SEL_BIND(LAW_ERR_FLO, "Numeric Overflow");
        SEL_BIND(LAW_ERR_OOM, "Out of Memory");
        SEL_BIND(LAW_ERR_SSL, "SSL I/O");
        SEL_BIND(LAW_ERR_MODE, "Bad Mode");
        SEL_BIND(LAW_ERR_GAI, "Get Address Info");
        SEL_BIND(LAW_ERR_WNTW, "Wants to Write");
        SEL_BIND(LAW_ERR_WNTR, "Wants to Read");
        SEL_BIND(LAW_ERR_TTL, "Timed Out");
}

enum pgc_err law_err_pgc;

enum pgc_err law_err_pgc_get()
{
        return law_err_pgc;
}

void law_err_pgc_set(enum pgc_err error)
{
        law_err_pgc = error;
}

int law_err_ssl;

int law_err_ssl_get()
{
        return law_err_ssl;
}

void law_err_ssl_set(const int error)
{
        law_err_ssl = error;
}