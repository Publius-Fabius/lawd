#ifndef LAWD_HTTP_HEADERS_H
#define LAWD_HTTP_HEADERS_H

#include <stddef.h>

/** HTTP Headers */
typedef struct law_htheaders law_htheaders_t;

/** HTTP Headers Iterator */
typedef struct law_hth_iter law_hth_iter_t;

/** Get the header value by its name.  Returns NULL if not found. */
const char *law_hth_get(
        law_htheaders_t *headers,
        const char *field_name);

/** Allocate and initialize an iterator for the header collection. */
law_hth_iter_t *law_hth_elems(
        law_htheaders_t *headers,
        void *(*alloc)(const size_t nbytes, void *state),
        void *state);

/**
 * Get the next header field <name, value> pair.  A NULL return value 
 * indicates the iterator has completed.  If NULL is returned, the values 
 * of (*field_name) and (*field_value) are undefined.
 */
law_hth_iter_t *law_hth_next(
        law_hth_iter_t *iterator,
        const char **field_name,
        const char **field_value);

#endif