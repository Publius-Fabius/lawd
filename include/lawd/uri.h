#ifndef LAWD_URI_H
#define LAWD_URI_H

#include "pgenc/parser.h"
#include "pgenc/stack.h"
#include "pgenc/ast.h"
#include "lawd/error.h"
#include <stddef.h>

/** URI Part */
enum law_uri_part {
        LAW_URI_SCHEME,                 /** URI Scheme */
        LAW_URI_HOST,                   /** URI Host */
        LAW_URI_PORT,                   /** URI Port */
        LAW_URI_PATH,                   /** URI Path */
        LAW_URI_QUERY,                  /** URI Query */
        LAW_URI_TOKEN,                  /** Query Token */
        LAW_URI_ELEM,                   /** Query Element */
        LAW_URI_SEG                     /** Path Segment */
};

/** URI Path */
typedef struct law_uri_path law_uri_path_t;

/** Path Iterator */
typedef struct law_uri_path_iter law_uri_path_iter_t;

/** URI Query */
typedef struct law_uri_query law_uri_query_t;

/** Query Iterator */
typedef struct law_uri_query_iter law_uri_query_iter_t;

/** Uniform Resource Identifier */
typedef struct law_uri {
        char *scheme;
        char *host;
        char *port;
        char *path;
        char *query;
} law_uri_t;

/** Get the number of segments in the path. */
size_t law_uri_path_count(law_uri_path_t *path);

/** 
 * Get the path segment at the given index.  Negative values index from 
 * the right instead of the left.
 */
const char *law_uri_path_at(
        law_uri_path_t *path, 
        const intptr_t index);

/** Initialize an iterator for the path's elements. */
law_uri_path_iter_t *law_uri_path_segs(
        law_uri_path_t *path,
        void *(*alloc)(const size_t nbytes, void *state),
        void *alloc_state);

/** Increment the path iterator. */
law_uri_path_iter_t *law_uri_path_next(
        law_uri_path_iter_t *path,
        const char **segment);

/** Get the number of query elements in the collection. */
size_t law_uri_query_count(law_uri_query_t *query);

/** Get the query value by name. */
const char *law_uri_query_get(
        law_uri_query_t *query, 
        const char *name);

/** Initialize an iterator for the query's elements. */
law_uri_query_iter_t *law_uri_query_elems(
        law_uri_query_t *query,
        void *(*alloc)(const size_t nbytes, void *state),
        void *alloc_state);

/** Increment the query iterator. */
law_uri_query_iter_t *law_uri_query_next(
        law_uri_query_iter_t *query,
        const char **name,
        const char **value);

/** Print out URI to FILE. */
sel_err_t law_uri_fprint(FILE *file, law_uri_t *uri);

/** Print out URI to IO buffer. */
sel_err_t law_uri_bprint(struct pgc_buf *buffer, law_uri_t *uri);

/** Populate the URI from an AST. */
struct law_uri *law_uri_from_ast(
        law_uri_t *uri,
        struct pgc_ast_lst *ast);

/** Parse the URI starting at the buffer's current offset. */
sel_err_t law_uri_parse(
        const struct pgc_par *parser,
        struct pgc_buf *buffer,
        struct pgc_stk *heap,
        law_uri_t *uri);

/** Parse the URI query string. */
sel_err_t law_uri_parse_query(
        char *query_string,
        struct pgc_stk *heap,
        law_uri_query_t **query);

/** Parse the URI path string. */
sel_err_t law_uri_parse_path(
        char *path_string,
        struct pgc_stk *heap,
        law_uri_path_t **path);

/*
 *                Implementation of RFC3986 
 *      Uniform Resource Identifier (URI): Generic Syntax
 *
 * scheme :// host : port / path ? query # fragment
 * 
 * GRAMMAR 
 * 
 * sub_delims  = "!" | "$" | "&" | "'" | "(" | ")"
 *             | "*" | "+" | "," | ";" | "="
 * 
 * gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@"
 * 
 * reserved = gen_delims | sub_delims
 * 
 * unreserved = ALPHA | DIGIT | "-" | "." | "_" | "~"
 * 
 * scheme = ALPHA *( ALPHA | DIGIT | "+" | "-" | "." )
 * 
 * dec-octet     = DIGIT                 ; 0-9
 *               | %x31-39 DIGIT         ; 10-99
 *               | "1" 2DIGIT            ; 100-199
 *               | "2" %x30-34 DIGIT     ; 200-249
 *               | "25" %x30-35          ; 250-255
 * 
 * IPv4address   = dec-octet "." dec-octet "." dec-octet "." dec-octet
 * 
 * h16         = 1*4HEXDIG
 *             ; 16 bits of address represented in hexadecimal
 * 
 * ls32        = ( h16 ":" h16 ) | IPv4address
 *             ; least-significant 32 bits of address
 * 
 * IPv6address =                               6( h16 ":" ) ls32
 *                |                       "::" 5( h16 ":" ) ls32
 *                | [               h16 ] "::" 4( h16 ":" ) ls32
 *                | [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
 *                | [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
 *                | [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
 *                | [ *4( h16 ":" ) h16 ] "::"              ls32
 *                | [ *5( h16 ":" ) h16 ] "::"              h16
 *                | [ *6( h16 ":" ) h16 ] "::"
 *
 * IP_literal = "[" ( IPv6address | IPvFuture  ) "]"
 *
 * IPvFuture = "v" 1*HEXDIG "." 1*( unreserved | sub-delims | ":" )
 * 
 * pct-encoded = "%" HEXDIG HEXDIG
 * 
 * reg_name = *( unreserved | pct_encoded | sub_delims )
 * 
 * host = IP_literal | IPv4address | reg_name
 * 
 * port = *DIGIT
 * 
 * authority = host [ ":" port ]
 * 
 * pchar = unreserved | pct_encoded | sub_delims | ":" | "@"
 * 
 * segment_nz_nc = 1*( unreserved | pct_encoded | sub_delims | "@" )
 *                  ; non-zero-length segment without any colon ":"
 *
 * segment_nz = 1*pchar
 * 
 * segment = *pchar
 * 
 * path_noscheme = segment_nz_nc *( "/" segment )
 * 
 * path_abempty  = *( "/" segment )
 * 
 * path_absolute = "/" [ segment_nz *( "/" segment ) ]
 * 
 * path_rootless = segment_nz *( "/" segment )
 * 
 * path_empty    = 0<pchar>
 * 
 * path          = path_abempty    ; begins with "/" or is empty
 *               | path_absolute   ; begins with "/" but not "//"
 *               | path_noscheme   ; begins with a non-colon segment
 *               | path_rootless   ; begins with a segment
 *               | path_empty      ; zero characters
 * 
 * hier_part = "//" authority path_abempty
 *              | path_absolute
 *              | path_rootless
 *              | path_empty
 * 
 * query = *( pchar | "/" | "?" )
 * 
 * fragment = *( pchar | "/" | "?" )
 * 
 * URI = scheme ":" hier_part [ "?" query ] [ "#" fragment ]
 *
 * relative_part = "//" authority path_abempty
 *                  | path_absolute
 *                  | path_noscheme
 *                  | path_empty
 * 
 * relative_ref  = relative_part [ "?" query ] [ "#" fragment ]
 * 
 * URI_reference = URI | relative_ref
 * 
 * absolute_URI = scheme ":" hier_part [ "?" query ]
 */

/** Capture scheme component. */
sel_err_t law_uri_cap_scheme(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture host component. */
sel_err_t law_uri_cap_host(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture port component. */
sel_err_t law_uri_cap_port(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture path component. */
sel_err_t law_uri_cap_path(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture query token. */
sel_err_t law_uri_cap_token(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture query element. */
sel_err_t law_uri_cap_elem(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

#endif