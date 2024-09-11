
#include "lawd/uri.h"
#include "pgenc/lang.h"
#include <string.h>
#include <stdlib.h>

struct law_uri_query {
        struct pgc_ast_lst *list;
};

struct law_uri_query_iter {
        struct pgc_ast_lst *list;
};

struct law_uri_path {
        struct pgc_ast_lst *list;
        size_t count;
};

struct law_uri_path_iter {
        struct pgc_ast_lst *list;
};

sel_err_t law_uri_init_from_ast(
        struct law_uri *uri,
        struct pgc_ast_lst *list)
{
        uri->scheme = NULL;
        uri->host = NULL;
        uri->port = NULL;
        uri->path = NULL;
        uri->query = NULL;
        for(struct pgc_ast_lst *i = list; i; i = i->nxt) {
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

sel_err_t law_uri_parse(
        struct pgc_par *parser,
        struct pgc_buf *buffer,
        struct pgc_stk *heap,
        struct law_uri *uri)
{
        struct pgc_ast_lst *result;
        enum pgc_err err = pgc_lang_parse(parser, buffer, heap, &result);
        if(err != PGC_ERR_OK) {
                return err;
        } else {
                return law_uri_init_from_ast(uri, result);
        }
}

struct pgc_par *law_uri_parsers_query_list(struct law_uri_parsers *pars);

sel_err_t law_uri_parse_query(
        char *text,
        struct pgc_stk *heap,
        struct law_uri_query **query)
{
        const size_t text_length = strlen(text);
        struct pgc_buf buf;
        pgc_buf_init(&buf, text, text_length, text_length);

        struct pgc_ast_lst *result;

        enum pgc_err err = pgc_lang_parse(
                law_uri_parsers_query_list(export_law_uri_parsers()),
                &buf,
                heap,
                &result);
        
        if(err != PGC_ERR_OK) {
                return err;
        }

        *query = pgc_stk_push(heap, sizeof(struct law_uri_query));
        (*query)->list = result;

        return SEL_ERR_OK;
}

size_t law_uri_query_count(
        struct law_uri_query *query)
{
        return pgc_ast_len(query->list);
}

const char *law_uri_query_lookup(
        struct law_uri_query *query, 
        char *name)
{
        static const char *EMPTY = "";
        for(struct pgc_ast_lst *l = query->list; l; l = l->nxt) {
                struct pgc_ast_lst *tuple = pgc_ast_tolst(l->val);
                if(strcmp(pgc_ast_tostr(tuple->val), name)) {
                        continue;
                } else if(tuple->nxt) {
                        return pgc_ast_tostr(tuple->nxt->val);
                } else {
                        return EMPTY;
                }
        }
        return NULL;
}

struct law_uri_query_iter *law_uri_query_elems(
        struct law_uri_query *query)
{
        struct law_uri_query_iter *iter = 
                malloc(sizeof(struct law_uri_query_iter));
        iter->list = query->list;
        return iter;
}

void law_uri_query_finish(
        struct law_uri_query_iter *query)
{
        free(query);
}

struct law_uri_query_iter *law_uri_query_next(
        struct law_uri_query_iter *query,
        const char **name,
        const char **value)
{
        static const char *EMPTY = "";
        if(!query->list) {
                law_uri_query_finish(query);
                return NULL;
        }
        struct pgc_ast_lst *lst = pgc_ast_tolst(query->list->val);
        *name = pgc_ast_tostr(lst->val);
        if(lst->nxt) {
                *value = pgc_ast_tostr(lst->nxt->val);
        } else {
                *value = EMPTY;
        }
        query->list = query->list->nxt;
        return query;
}

struct pgc_par *law_uri_parsers_path_list(struct law_uri_parsers *pars);

sel_err_t law_uri_parse_path(
        char *text,
        struct pgc_stk *heap,
        struct law_uri_path **path)
{
        const size_t text_length = strlen(text);
        struct pgc_buf buf;
        pgc_buf_init(&buf, text, text_length, text_length);

        struct pgc_ast_lst *result;

        enum pgc_err err = pgc_lang_parse(
                law_uri_parsers_path_list(export_law_uri_parsers()),
                &buf,
                heap,
                &result);
        
        if(err != PGC_ERR_OK) {
                return err;
        }

        *path = pgc_stk_push(heap, sizeof(struct law_uri_path));
        (*path)->list = result;
        (*path)->count = pgc_ast_len(result);

        return SEL_ERR_OK;
}

size_t law_uri_path_count(
        struct law_uri_path *path)
{
        return path->count;
}

const char *law_uri_path_at(
        struct law_uri_path *path, 
        const ssize_t index)
{
        const size_t i = index < 0 
                ? (size_t)((ssize_t)path->count + index) 
                : (size_t)index; 
        return pgc_ast_tostr(pgc_ast_at(path->list, i)->val);
}


struct law_uri_path_iter *law_uri_path_segs(
        struct law_uri_path *path)
{
        struct law_uri_path_iter *iter = 
                malloc(sizeof(struct law_uri_path_iter));
        iter->list = path->list;
        return iter;
}

void law_uri_path_finish(struct law_uri_path_iter *iter)
{
        free(iter);
}

struct law_uri_path_iter *law_uri_path_next(
        struct law_uri_path_iter *iter,
        const char **segment)
{
        if(!iter->list) {
                law_uri_path_finish(iter);
                return NULL;
        }
        *segment = pgc_ast_tostr(iter->list->val);
        iter->list = iter->list->nxt;
        return iter;
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

enum pgc_err law_uri_cap_token(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_TOKEN, pgc_lang_readstr);
}

enum pgc_err law_uri_cap_elem(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readexp(
                buffer, state, arg, LAW_URI_ELEM, 0);
}

enum pgc_err law_uri_cap_seg(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg)
{
        return pgc_lang_readterm(
                buffer, state, arg, LAW_URI_SEG, pgc_lang_readstr);
}