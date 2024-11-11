
#include "lawd/error.h"
#include "lawd/http/headers.h"
#include <string.h>

struct law_hthdrs {
        struct pgc_ast_lst *list;
};

struct law_hthdrs_iter {
        struct pgc_ast_lst *list;
};

struct law_hthdrs *law_hth_from_ast(
        struct pgc_ast_lst *list,
        void *(alloc)(void*, const size_t),
        void *st)
{
        struct law_hthdrs *hdrs = alloc(st, sizeof(struct law_hthdrs));
        if(!hdrs) return NULL;
        hdrs->list = list;
        return hdrs;
}

const char *law_hth_get(
        struct law_hthdrs *hdrs,
        const char *field_name)
{
        for(struct pgc_ast_lst *l = hdrs->list; l; l = l->nxt) {
                struct pgc_ast_lst *tpl = pgc_ast_tolst(l->val);
                if(!strcmp(pgc_ast_tostr(tpl->val), field_name)) 
                        return tpl->nxt ? pgc_ast_tostr(tpl->nxt->val) : "";
        }
        return NULL;
}

struct law_hthdrs_iter *law_hth_elems(
        struct law_hthdrs *hdrs,
        void *(alloc)(void*, const size_t),
        void *st)
{
        struct law_hthdrs_iter *iter = alloc(st, sizeof(struct law_hthdrs_iter));
        if(!iter) return NULL;
        iter->list = hdrs->list;
        return iter;
}

struct law_hthdrs_iter *law_hth_next(
        struct law_hthdrs_iter *iter,
        const char **field_name,
        const char **field_value)
{
        if(!iter->list) return NULL;
        struct pgc_ast_lst *ele = pgc_ast_tolst(iter->list->val);
        if(field_name) 
                *field_name = pgc_ast_tostr(ele->val);
        if(field_value) 
                *field_value = ele->nxt ? pgc_ast_tostr(ele->nxt->val) : "";
        iter->list = iter->list->nxt;
        return iter;
}
