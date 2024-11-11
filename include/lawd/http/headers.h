#ifndef LAWD_HTTP_HEADERS_H
#define LAWD_HTTP_HEADERS_H

#include "pgenc/ast.h"
#include <stddef.h>

/** HTTP Headers */
struct law_hthdrs;

/** HTTP Headers Iterator */
struct law_hthdrs_iter;

/** Generate headers from an abstract syntax tree. */
struct law_hthdrs *law_hth_from_ast(
        struct pgc_ast_lst *list,
        void *(*alloc)(void *state, const size_t nbytes),
        void *alloc_state);

/** Get the header value by its name.  Returns NULL if not found. */
const char *law_hth_get(
        struct law_hthdrs *headers,
        const char *field_name);

/** Initialize an iterator for the header collection. */
struct law_hthdrs_iter *law_hth_elems(
        struct law_hthdrs *headers,
        void *(*alloc)(void *state, const size_t nbytes),
        void *alloc_state);

/**
 * Get the next header field <name, value> pair.  A NULL return value 
 * indicates the iterator has completed.  If NULL is returned, the values 
 * of (*field_name) and (*field_value) are undefined.
 */
struct law_hthdrs_iter *law_hth_next(
        struct law_hthdrs_iter *iterator,
        const char **field_name,
        const char **field_value);

#endif