#ifndef LAWD_PRIVATE_HTTP_HEADERS_H
#define LAWD_PRIVATE_HTTP_HEADERS_H

#include "lawd/http_headers.h"
#include "pgenc/ast.h"

struct law_htheaders {
        struct pgc_ast_lst *list;
};

struct law_hth_iter {
        struct pgc_ast_lst *list;
};

#endif