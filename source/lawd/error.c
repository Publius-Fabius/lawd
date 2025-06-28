
#include "lawd/error.h"
#include <string.h>
#include <threads.h>

void law_err_init()
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

        SEL_BIND(LAW_ERR_MODE, "Bad Mode");
        SEL_BIND(LAW_ERR_GAI, "Get Address Info");
        SEL_BIND(LAW_ERR_WANTW, "Wants to Write");
        SEL_BIND(LAW_ERR_WANTR, "Wants to Read");
        SEL_BIND(LAW_ERR_TIMEOUT, "Timed Out");
        SEL_BIND(LAW_ERR_LIMIT, "Limit Reached");
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

        if(law_errors.offset == 0) 
                return false;

        if(info) 
                *info = law_errors.array[--law_errors.offset];

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

        if(law_errors.offset == LAW_ERR_STACK_MAX) {
                return type;
        }

        law_err_info_t *info = law_errors.array + law_errors.offset++;
        memset(info, 0, sizeof(law_err_info_t));

        info->type = type;
        info->action = action;
        info->file = file;
        info->function = function;
        info->line = line;

        return type;
}

sel_err_t law_err_fprint(FILE *stream, law_err_info_t *info)
{
        if(fprintf(stream, 
                "error-type: %s \r\n"
                "action: %s \r\n"
                "file: %s \r\n"
                "function: %s \r\n"
                "line: %i \r\n",
                sel_strerror(info->type),
                info->action,
                info->file,
                info->function,
                info->line) < 0) 
        {
                return LAW_ERR_SYS;
        }
        return LAW_ERR_OK;
}