#ifndef LAWD_ERROR_H
#define LAWD_ERROR_H

#include "selc/error.h"

/** Error Type */
enum law_err_type {
        LAW_ERR_TRY_AGAIN               = -100, /** Try Again */
        LAW_ERR_BAD_MODE                = -101, /** Bad Mode */
        LAW_ERR_BUFFER_LIMIT            = -102, /** Buffer Limit Reached */
        LAW_ERR_ENCOUNTERED_EOF         = -103, /** Encountered EOF */
        LAW_ERR_SOCKET_IO               = -104, /** Socket IO Error */
        LAW_ERR_MALFORMED_MESSAGE       = -105, /** Malformed Message */
        LAW_ERR_METHOD_NOT_ALLOWED      = -106, /** Method Not Allowed */
        LAW_ERR_UNSUPPORTED_VERSION     = -107, /** Unsupported Version */
        LAW_ERR_HEAP_EXHAUSTED          = -108  /** Heap out of memory */
};

void law_err_init();

#endif