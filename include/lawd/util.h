#ifndef LAWD_UTIL_H
#define LAWD_UTIL_H

#include "lawd/error.h"
#include "pgenc/buffer.h"
#include <stddef.h>

sel_err_t law_util_scan(
    struct pgc_buf *buffer,
    char *bytes,
    const size_t length);

#endif