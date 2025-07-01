#ifndef LAWD_ERROR_H
#define LAWD_ERROR_H

#include "selc/error.h"
#include "pgenc/error.h"
#include <stdbool.h>
#include <stdio.h>

/** Error Type */
enum law_err_type {
        
        /* SEL Errors */

        LAW_ERR_OK      = SEL_ERR_OK,           /** All OK */
        LAW_ERR_SYS     = SEL_ERR_SYS,          /** Syscall Error */

        /* PGENC Errors */

        LAW_ERR_OOB     = PGC_ERR_OOB,          /** Out of Bounds Error */
        LAW_ERR_CMP     = PGC_ERR_CMP,          /** Comparison Error */
        LAW_ERR_ENC     = PGC_ERR_ENC,          /** Encoding Error  */
        LAW_ERR_SYN     = PGC_ERR_SYN,          /** Syntax Error */
        LAW_ERR_FLO     = PGC_ERR_FLO,          /** Numeric Overflow Error */
        LAW_ERR_OOM     = PGC_ERR_OOM,          /** Out of Memory Error */

        /* Control Errors */

        LAW_ERR_MODE                            = -1700,             
        LAW_ERR_GAI                             = -1702,               
        LAW_ERR_WANTW                           = -1703,            
        LAW_ERR_WANTR                           = -1704,            
        LAW_ERR_TIME                            = -1705,               
        LAW_ERR_LIMIT                           = -1706,            
        LAW_ERR_EOF                             = -1707,
        LAW_ERR_SSL                             = -1708,
        LAW_ERR_VERS                            = -1709,
        LAW_ERR_NOID                            = -1710,

        /* HTTP Errors */

        LAW_ERR_SSL_ACCEPT_FAILED               = -1800,
        LAW_ERR_SSL_SHUTDOWN_FAILED             = -1801,
        LAW_ERR_REQLINE_TOO_LONG                = -1802,
        LAW_ERR_HEADERS_TOO_LONG                = -1803,
        LAW_ERR_REQLINE_MALFORMED               = -1804,
        LAW_ERR_HEADERS_MALFORMED               = -1805,
        LAW_ERR_REQLINE_TIMEOUT                 = -1806,
        LAW_ERR_HEADERS_TIMEOUT                 = -1807,
        LAW_ERR_SSL_SHUTDOWN_TIMEOUT            = -1808
};

typedef struct law_err_info {                   /** Error Info */
        sel_err_t type;                         /** Error Type */
        const char *action;                     /** Action String */
        const char *file;                       /** Source File Name */
        const char *function;                   /** Function Name */
        int line;                               /** Line Number */
} law_err_info_t;

/** 
 * Clear the error stack. 
 */
void law_err_clear();

/**
 * Get a copy of the bottom-most error on the thread stack.
 */
bool law_err_bottom(law_err_info_t *info);

/**
 * Peek the top-most error on the thread stack.
 */
bool law_err_peek(law_err_info_t *info);

/** 
 * Pop the top-most error from the error stack. 
 */
bool law_err_pop(law_err_info_t *info);

/** 
 * Push an error to the error stack. 
 */
sel_err_t law_err_push(
        const sel_err_t type,
        const char *action,
        const char *file,
        const char *function,
        const int line);

/** 
 * Push error info to the error stack.
 */
#define LAW_ERR_PUSH(TYPE, ACTION) \
        law_err_push((TYPE), (ACTION), __FILE__, __func__, __LINE__)

/**
 * Print out error info.
 * 
 * = 01fa LAW_ERR_OOB bounds_check push_task():server.c:215 
 * 
 */
sel_err_t law_err_print(FILE *stream, law_err_info_t *info);

/**
 * Print stack trace.
 */
sel_err_t law_err_print_stack(FILE *stream);

/** 
 * Initialize errors. 
 */
void law_err_init();

#endif