
#include "lawd/http/parse.h"
#include "pgenc/lang.h"

static sel_err_t law_htp_read_uint32(
        struct pgc_buf *buffer,
        void *state,
        const size_t start,
        const int16_t tag)
{
        return pgc_lang_readenc(
                buffer, 
                state, 
                start,
                PGC_AST_UINT32, 
                tag, 
                10, 
                pgc_buf_decode_dec, 
                pgc_buf_decode_uint32);
}

sel_err_t law_htp_cap_status(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_HTP_STATUS, law_htp_read_uint32);
}

sel_err_t law_htp_cap_method(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_HTP_METHOD, pgc_lang_readstr);
}

sel_err_t law_htp_cap_version(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_HTP_VERSION, pgc_lang_readstr);
}

sel_err_t law_htp_cap_field_name(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_HTP_FIELD_NAME, pgc_lang_readstr);
}

sel_err_t law_htp_cap_field_value(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_HTP_FIELD_VALUE, pgc_lang_readstr);
}

sel_err_t law_htp_cap_field(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readexp(stk, buf, state, arg, LAW_HTP_FIELD, 0);
}

sel_err_t law_htp_cap_origin_form(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *st,
        const struct pgc_par *arg)
{
        return pgc_lang_readexp(stk, buf, st, arg, LAW_HTP_ORIGIN_FORM, 0);
}

sel_err_t law_htp_cap_absolute_form(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *st,
        const struct pgc_par *arg)
{
        return pgc_lang_readexp(stk, buf, st, arg, LAW_HTP_ABSOLUTE_FORM, 0);
}

sel_err_t law_htp_cap_authority_form(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *st,
        const struct pgc_par *arg)
{
        return pgc_lang_readexp(stk, buf, st, arg, LAW_HTP_AUTHORITY_FORM, 0);
}
