
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

sel_err_t law_uri_fprint(FILE *file, law_uri_t *uri)
{
        if(uri->scheme) SEL_IO_QUIETLY(fprintf(file, "%s://", uri->scheme));
        if(uri->host) SEL_IO_QUIETLY(fprintf(file, "%s", uri->host));
        if(uri->path) SEL_IO_QUIETLY(fprintf(file, "%s", uri->path));
        if(uri->query) SEL_IO_QUIETLY(fprintf(file, "?%s", uri->query));
        return LAW_ERR_OK;
}

sel_err_t law_uri_bprint(struct pgc_buf *buf, law_uri_t *uri)
{
        if(uri->scheme) 
                SEL_TRY_QUIETLY(pgc_buf_printf(buf, "%s://", uri->scheme));
        if(uri->host) 
                SEL_TRY_QUIETLY(pgc_buf_printf(buf, "%s", uri->host));
        if(uri->path) 
                SEL_TRY_QUIETLY(pgc_buf_printf(buf, "%s", uri->path));
        if(uri->query) 
                SEL_TRY_QUIETLY(pgc_buf_printf(buf, "?%s", uri->query));
        return LAW_ERR_OK;
}

law_uri_t *law_uri_from_ast(
        law_uri_t *uri,
        struct pgc_ast_lst *list)
{
        memset(uri, 0, sizeof(law_uri_t));
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
                        default: SEL_HALT();
                }
        }
        return uri;
}

sel_err_t law_uri_parse(
        const struct pgc_par *parser,
        struct pgc_buf *buffer,
        struct pgc_stk *heap,
        law_uri_t *uri)
{
        struct pgc_ast_lst *result;
        SEL_TRY_QUIETLY(pgc_lang_parse(parser, buffer, heap, &result));
        law_uri_from_ast(uri, result);
        return LAW_ERR_OK;
}

extern const struct pgc_par law_uri_par_query_list;

sel_err_t law_uri_parse_query(
        char *text,
        struct pgc_stk *heap,
        law_uri_query_t **query)
{
        const size_t text_length = strlen(text);
        struct pgc_buf buf;
        pgc_buf_init(&buf, text, text_length, text_length);

        struct pgc_ast_lst *result;

        sel_err_t err = pgc_lang_parse(
                &law_uri_par_query_list,
                &buf,
                heap,
                &result);
        
        if(err != PGC_ERR_OK) return err;
        
        *query = pgc_stk_push(heap, sizeof(law_uri_query_t));
        (*query)->list = result;

        return SEL_ERR_OK;
}

size_t law_uri_query_count(
        law_uri_query_t *query)
{
        return pgc_ast_len(query->list);
}

const char *law_uri_query_get(
        law_uri_query_t *query, 
        const char *name)
{
        for(struct pgc_ast_lst *l = query->list; l; l = l->nxt) {
                struct pgc_ast_lst *tpl = pgc_ast_tolst(l->val);
                if(!strcmp(pgc_ast_tostr(tpl->val), name))
                        return tpl->nxt ? pgc_ast_tostr(tpl->nxt->val) : "";
        }
        return NULL;
}

law_uri_query_iter_t *law_uri_query_elems(
        law_uri_query_t *query,
        void *(*alloc)(const size_t nbytes, void *state),
        void *alloc_state)
{
        law_uri_query_iter_t *iter = alloc(
                sizeof(law_uri_query_iter_t),
                alloc_state);
        if(!iter) return NULL;
        iter->list = query->list;
        return iter;
}

law_uri_query_iter_t *law_uri_query_next(
        law_uri_query_iter_t *query,
        const char **name,
        const char **value)
{
        if(!query->list) return NULL;
        struct pgc_ast_lst *lst = pgc_ast_tolst(query->list->val);
        if(name) *name = pgc_ast_tostr(lst->val);
        if(value) *value = lst->nxt ? pgc_ast_tostr(lst->nxt->val) : "";
        query->list = query->list->nxt;
        return query;
}

extern const struct pgc_par law_uri_par_path_list;

sel_err_t law_uri_parse_path(
        char *text,
        struct pgc_stk *heap,
        law_uri_path_t **path)
{
        const size_t text_length = strlen(text);
        struct pgc_buf buf;
        pgc_buf_init(&buf, text, text_length, text_length);

        struct pgc_ast_lst *result;

        sel_err_t err = pgc_lang_parse(
                &law_uri_par_path_list,
                &buf,
                heap,
                &result);
        if(err != PGC_ERR_OK) return err;

        *path = pgc_stk_push(heap, sizeof(law_uri_path_t));
        (*path)->list = result;
        (*path)->count = pgc_ast_len(result);

        return SEL_ERR_OK;
}

size_t law_uri_path_count(law_uri_path_t *path)
{
        return path->count;
}

const char *law_uri_path_at(
        law_uri_path_t *path, 
        const intptr_t index)
{
        const size_t i = index < 0 
                ? (size_t)((intptr_t)path->count + index) 
                : (size_t)index; 
        return pgc_ast_tostr(pgc_ast_at(path->list, i)->val);
}

law_uri_path_iter_t *law_uri_path_segs(
        law_uri_path_t *path,
        void *(*alloc)(const size_t nbytes, void *state),
        void *alloc_state)
{
        law_uri_path_iter_t *iter = alloc(
                sizeof(law_uri_path_iter_t),
                alloc_state);
        if(!iter) return NULL;
        iter->list = path->list;
        return iter;
}

law_uri_path_iter_t *law_uri_path_next(
        law_uri_path_iter_t *iter,
        const char **segment)
{
        if(!iter->list) return NULL;
        if(segment) *segment = pgc_ast_tostr(iter->list->val);
        iter->list = iter->list->nxt;
        return iter;
}

sel_err_t law_uri_cap_scheme(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_SCHEME, pgc_lang_readstr);
}

sel_err_t law_uri_cap_host(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_HOST, pgc_lang_readstr);
}

sel_err_t law_uri_cap_port(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_PORT, pgc_lang_readstr);
}

sel_err_t law_uri_cap_path(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_PATH, pgc_lang_readstr);
}

sel_err_t law_uri_cap_query(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_QUERY, pgc_lang_readstr);
}

sel_err_t law_uri_cap_token(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_TOKEN, pgc_lang_readstr);
}

sel_err_t law_uri_cap_elem(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readexp(
                stk, buf, state, arg, LAW_URI_ELEM, 0);
}

sel_err_t law_uri_cap_seg(
        struct pgc_stk *stk,
        struct pgc_buf *buf,
        void *state,
        const struct pgc_par *arg)
{
        return pgc_lang_readterm(
                stk, buf, state, arg, LAW_URI_SEG, pgc_lang_readstr);
}