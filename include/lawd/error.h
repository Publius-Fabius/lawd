#ifndef LAWD_ERROR_H
#define LAWD_ERROR_H

#include "selc/error.h"
#include "pgenc/error.h"

/** Error Type */
enum law_err_type {
        
        /* SEL Errors */

        LAW_ERR_OK      = SEL_ERR_OK,           /** All OK */
        LAW_ERR_SYS     = SEL_ERR_SYS,          /** Syscall Error */

        /* PGENC Errors */

        LAW_ERR_OOB     = PGC_ERR_OOB,          /** Out of Bounds Error */
        LAW_ERR_CMP     = PGC_ERR_CMP,          /** Comparison Error */
        LAW_ERR_ENC     = PGC_ERR_ENC,          /** Encoding Error  */
        LAW_ERR_EOF     = PGC_ERR_EOF,          /** End of File Encountered */
        LAW_ERR_SYN     = PGC_ERR_SYN,          /** Syntax Error */
        LAW_ERR_FLO     = PGC_ERR_FLO,          /** Numeric Overflow Error */
        LAW_ERR_OOM     = PGC_ERR_OOM,          /** Out of Memory Error */

        /* LAWD Errors */

        LAW_ERR_AGAIN   = -1700,                /** Try Again */
        LAW_ERR_MODE    = -1701,                /** Bad Mode */
        LAW_ERR_METH    = -1702,                /** Method Not Allowed */
        LAW_ERR_VERS    = -1703,                /** Unsupported Version */
        LAW_ERR_GAI     = -1704                 /** Address Lookup Error */
};

void law_err_init();

#endif