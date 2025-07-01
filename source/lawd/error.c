
#include "lawd/error.h"
#include <string.h>
#include <threads.h>
#include <errno.h>

void law_err_init()
{
        sel_init();

        SEL_BIND(LAW_ERR_OK, "LAW_ERR_OK");
        SEL_BIND(LAW_ERR_SYS, "LAW_ERR_SYS");

        SEL_BIND(LAW_ERR_OOB, "LAW_ERR_OOB");
        SEL_BIND(LAW_ERR_CMP, "LAW_ERR_CMP");
        SEL_BIND(LAW_ERR_ENC, "LAW_ERR_ENC");
        SEL_BIND(LAW_ERR_SYN, "LAW_ERR_SYN");
        SEL_BIND(LAW_ERR_FLO, "LAW_ERR_FLO");
        SEL_BIND(LAW_ERR_OOM, "LAW_ERR_OOM");

        SEL_BIND(LAW_ERR_MODE, "LAW_ERR_MODE");
        SEL_BIND(LAW_ERR_GAI, "LAW_ERR_GAI");
        SEL_BIND(LAW_ERR_WANTW, "LAW_ERR_WANTW");
        SEL_BIND(LAW_ERR_WANTR, "LAW_ERR_WANTR");
        SEL_BIND(LAW_ERR_TIME, "LAW_ERR_TIME");
        SEL_BIND(LAW_ERR_LIMIT, "LAW_ERR_LIMIT");
        SEL_BIND(LAW_ERR_EOF, "LAW_ERR_EOF");
        SEL_BIND(LAW_ERR_SSL, "LAW_ERR_SSL");
        SEL_BIND(LAW_ERR_VERS, "LAW_ERR_VERS");
        SEL_BIND(LAW_ERR_NOID, "LAW_ERR_NOID");

        SEL_BIND(LAW_ERR_SSL_ACCEPT_FAILED, "LAW_ERR_SSL_ACCEPT_FAILED");
        SEL_BIND(LAW_ERR_SSL_SHUTDOWN_FAILED, "LAW_ERR_SSL_SHUTDOWN_FAILED");
        SEL_BIND(LAW_ERR_REQLINE_TOO_LONG, "LAW_ERR_REQLINE_TOO_LONG");
        SEL_BIND(LAW_ERR_HEADERS_TOO_LONG, "LAW_ERR_HEADERS_TOO_LONG");
        SEL_BIND(LAW_ERR_REQLINE_MALFORMED, "LAW_ERR_REQLINE_MALFORMED");
        SEL_BIND(LAW_ERR_HEADERS_MALFORMED, "LAW_ERR_HEADERS_MALFORMED");

        SEL_BIND(LAW_ERR_REQLINE_TIMEOUT, "LAW_ERR_REQLINE_TIMEOUT");
        SEL_BIND(LAW_ERR_HEADERS_TIMEOUT, "LAW_ERR_HEADERS_TIMEOUT");
        SEL_BIND(LAW_ERR_SSL_SHUTDOWN_TIMEOUT, "LAW_ERR_SSL_SHUTDOWN_TIMEOUT");
}

thread_local law_err_info_t law_err_info;

#define LAW_ERR_STACK_MAX 16

typedef struct law_err_stack {

        law_err_info_t array[LAW_ERR_STACK_MAX];

        size_t offset;

} law_err_stack_t;

thread_local law_err_stack_t law_errors;

void law_err_clear()
{
        SEL_ASSERT(law_errors.offset <= LAW_ERR_STACK_MAX);

        law_errors.offset = 0;
}

bool law_err_pop(law_err_info_t *info) 
{
        SEL_ASSERT(law_errors.offset <= LAW_ERR_STACK_MAX);

        if(law_errors.offset == 0) return false;

        if(info) *info = law_errors.array[--law_errors.offset];

        return true;
}

sel_err_t law_err_push(
        const sel_err_t type,
        const char *action,
        const char *file,
        const char *function,
        const int line)
{
        SEL_ASSERT(law_errors.offset <= LAW_ERR_STACK_MAX);

        if(law_errors.offset == LAW_ERR_STACK_MAX) 
                return type;

        law_err_info_t *info = law_errors.array + law_errors.offset++;
        memset(info, 0, sizeof(law_err_info_t));

        info->type = type;
        info->action = action;
        info->file = file;
        info->function = function;
        info->line = line;

        return type;
}

sel_err_t law_err_print(FILE *stream, law_err_info_t *info)
{
        if(fprintf(stream,
                "= 0x%zx %s %s %s() : %s : %i\r\n",
                thrd_current(),
                sel_strerror(info->type),
                info->action,
                info->function,
                info->file,
                info->line) == -1)
                return LAW_ERR_SYS;

        return LAW_ERR_OK;
}

sel_err_t law_err_print_stack(FILE *stream)
{
        law_err_info_t info;

        while(law_err_pop(&info)) {
                if(law_err_print(stream, &info) != LAW_ERR_OK) 
                        return LAW_ERR_SYS;
        }

        return LAW_ERR_OK;
}
