#ifndef LAWD_ERROR_H
#define LAWD_ERROR_H

#include "selc/error.h"

/** Error Type */
enum law_err_type {
        LAW_ERR_AGAIN                   = -100, /** Try Again */
        LAW_ERR_MODE                    = -101, /** Bad Mode */
        LAW_ERR_BUFFER_LIMIT            = -102, /** Buffer Limit Reached */
        LAW_ERR_UNEXPECTED_EOF          = -103, /** Unexpected EOF */
        LAW_ERR_SOCKET_IO               = -104, /** Socket IO Error */
        LAW_ERR_MALFORMED_HEAD          = -105, /** Malformed Head */
        LAW_ERR_UNKNOWN_METHOD          = -106, /** Unknown Method */
        LAW_ERR_UNSUPPORTED_VERSION     = -107  /** Unsupported Version */
};

void law_err_init();

#endif