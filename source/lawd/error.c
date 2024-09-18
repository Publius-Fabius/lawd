
#include "lawd/error.h"

void law_err_init()
{
        SEL_BIND(LAW_ERR_OK, "All Ok");
        SEL_BIND(LAW_ERR_SYS, "System Error");
        SEL_BIND(LAW_ERR_OOB, "Out of Bounds Error");
        SEL_BIND(LAW_ERR_CMP, "Comparison Error");
        SEL_BIND(LAW_ERR_ENC, "Encoding Error");
        SEL_BIND(LAW_ERR_EOF, "End of File Encountered");
        SEL_BIND(LAW_ERR_SYN, "Syntax Error");
        SEL_BIND(LAW_ERR_FLO, "Numeric Overflow Error");
        SEL_BIND(LAW_ERR_OOM, "Out of Memory Error");
        SEL_BIND(LAW_ERR_AGAIN, "Try Operation Again");
        SEL_BIND(LAW_ERR_MODE, "Undefined Mode Error");
        SEL_BIND(LAW_ERR_METH, "Method Not Allowed");
        SEL_BIND(LAW_ERR_VERS, "Version Not Supported");
        SEL_BIND(LAW_ERR_GAI, "Get Address Info Error");
}
