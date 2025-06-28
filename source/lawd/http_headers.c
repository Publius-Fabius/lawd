
#include "lawd/error.h"
#include "lawd/http_headers.h"
#include "lawd/private/http_headers.h"
#include "pgenc/ast.h"
#include <string.h>

const char *law_hth_get(
        law_htheaders_t *hdrs,
        const char *field_name)
{
        for(struct pgc_ast_lst *l = hdrs->list; l; l = l->nxt) {
                struct pgc_ast_lst *tpl = pgc_ast_tolst(l->val);
                if(!strcmp(pgc_ast_tostr(tpl->val), field_name)) 
                        return tpl->nxt ? pgc_ast_tostr(tpl->nxt->val) : "";
        }
        return NULL;
}

law_hth_iter_t *law_hth_elems(
        struct law_htheaders *hdrs,
        void *(alloc)(const size_t, void*),
        void *st)
{
        law_hth_iter_t *iter = alloc(sizeof(law_hth_iter_t), st);
        if(!iter) return NULL;
        iter->list = hdrs->list;
        return iter;
}

law_hth_iter_t *law_hth_next(
        law_hth_iter_t *iter,
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
