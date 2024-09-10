
#include "lawd/uri.h"
#include "pgenc/lang.h"

sel_err_t law_uri_parse(
        struct pgc_par *parser,
        struct pgc_buf *buffer,
        struct pgc_stk *heap,
        struct law_uri *uri)
{
        struct pgc_ast_lst *result;

        uri->scheme = NULL;
        uri->host = NULL;
        uri->port = NULL;
        uri->path = NULL;
        uri->query = NULL;

        enum pgc_err err = pgc_lang_parse(parser, buffer, heap, &result);
        if(err != PGC_ERR_OK) {
                return err;
        }
        for(struct pgc_ast_lst *i = result; i; i = i->nxt) {
                switch(pgc_syn_typeof(i->val)) {
                        case LAW_URI_HOST:
                                uri->host = pgc_ast_tostr(i->val);
                                break;
                        case LAW_URI_PATH:
                                uri->path = pgc_ast_tostr(i->val);
                                break;
                        case LAW_URI_PORT:
                                uri->port = pgc_ast_tostr(i->val);
                                break;
                        case LAW_URI_SCHEME:
                                uri->scheme = pgc_ast_tostr(i->val);
                                break;
                        case LAW_URI_QUERY:
                                uri->query = pgc_ast_tostr(i->val);
                                break;
                        default: SEL_ABORT();
                }
        }

        return SEL_ERR_OK;
}

enum pgc_err law_uri_capscheme(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_SCHEME, pgc_lang_readstr);
}

enum pgc_err law_uri_cap_scheme(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_SCHEME, pgc_lang_readstr);
}

enum pgc_err law_uri_cap_host(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_HOST, pgc_lang_readstr);
}

enum pgc_err law_uri_cap_port(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_PORT, pgc_lang_readstr);
}

enum pgc_err law_uri_cap_path(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_PATH, pgc_lang_readstr);
}

enum pgc_err law_uri_cap_query(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_QUERY, pgc_lang_readstr);
}