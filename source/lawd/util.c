
#include "lawd/util.h"

sel_err_t law_util_scan(
        struct pgc_buf *buf,
        char *bytes,
        const size_t nbytes)
{
        for(;;) {
                size_t beg = pgc_buf_tell(buf);
                sel_err_t err = pgc_buf_cmp(buf, bytes, nbytes);
                switch(err) {
                        case PGC_ERR_OK:
                                PGC_TRY_QUIETLY(pgc_buf_seek(buf, beg));
                                return SEL_ERR_OK;
                        case PGC_ERR_CMP: 
                                PGC_TRY_QUIETLY(pgc_buf_seek(buf, beg + 1));
                                break;
                        default: 
                                PGC_TRY_QUIETLY(pgc_buf_seek(buf, beg));
                                return err;
                }
        }
}