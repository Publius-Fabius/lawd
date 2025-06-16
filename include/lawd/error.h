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
        LAW_ERR_SSL     = PGC_ERR_SSL,          /** SSL Error */

        /* LAWD Errors */

        LAW_ERR_MODE    = -1700,                /** Bad Mode */
        LAW_ERR_NOID    = -1701,                /** Id Does Not Exist */
        LAW_ERR_GAI     = -1702,                /** Address Lookup Error */
        LAW_ERR_WNTW    = -1703,                /** Wants to Write */
        LAW_ERR_WNTR    = -1704,                /** Wants to Read */
        LAW_ERR_TTL     = -1705,                /** Timeout Error */
        LAW_ERR_LIM     = -1706,                /** Limit Reached */
        LAW_ERR_WS      = -1707                 /** Web Socket Error */
        
};

/** 
 * Initialize LAWD errors. 
 */
void law_error_init();

#endif