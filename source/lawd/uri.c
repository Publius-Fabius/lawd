
#include "lawd/uri.h"
#include "pgenc/lang.h"

enum law_uri_type {
        LAW_URI_SCHEME
};

enum pgc_err law_uri_capscheme(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_SCHEME, pgc_lang_readstr);
}