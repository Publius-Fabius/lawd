#ifndef LAWD_ERROR_H
#define LAWD_ERROR_H

#include "selc/error.h"

/** Error Type */
enum law_err_type {
        LAW_ERR_AGAIN    = -100,         /** Try Again */
        LAW_ERR_MODE     = -101          /** Bad Mode */
};

void law_err_init();

#endif