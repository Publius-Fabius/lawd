
/** 
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

#ifndef LAWD_URI_H
#define LAWD_URI_H

#include "pgenc/parser.h"
#include "pgenc/stack.h"
#include "pgenc/ast.h"
#include "lawd/error.h"
#include <stddef.h>

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
struct law_uri_path;

/** Path Iterator */
struct law_uri_path_i;

/** URI Query */
struct law_uri_query;

/** Query Iterator */
struct law_uri_query_i;

/** Uniform Resource Identifier */
struct law_uri {
        char *scheme;
        char *host;
        char *port;
        char *path;
        char *query;
};

/** 
 * Get the number of segments in the path.
 */
size_t law_uri_path_count(
        struct law_uri_path *path);

/** 
 * Get the path segment at the given index.  Negative values index from 
 * the right instead of the left.
 */
const char *law_uri_path_at(
        struct law_uri_path *path, 
        const ssize_t index);

/**
 * Get an iterator for the path's elements.
 */
struct law_uri_path_i *law_uri_path_segs(
        struct law_uri_path *path);

/**
 * Finish an incomplete iteration.
 */
void law_uri_path_i_free(
        struct law_uri_path_i *path);

/**
 * Increment the path iterator.
 */
struct law_uri_path_i *law_uri_path_i_next(
        struct law_uri_path_i *path,
        const char **segment);

/**
 * Get the number of query elements in the collection.
 */
size_t law_uri_query_count(struct law_uri_query *query);

/** 
 * Get the query value by name. 
 */
const char *law_uri_query_get(
        struct law_uri_query *query, 
        char *name);

/**
 * Get an iterator for the query's elements.
 */
struct law_uri_query_i *law_uri_query_elems(
        struct law_uri_query *query);

/**
 * Finish an incomplete iteration.
 */
void law_uri_query_i_free(
        struct law_uri_query_i *query);

/**
 * Increment the query iterator.
 */
struct law_uri_query_i *law_uri_query_i_next(
        struct law_uri_query_i *query,
        const char **name,
        const char **value);

/** 
 * Print out URI to FILE.
 */
sel_err_t law_uri_fprint(FILE *file, struct law_uri *uri);

/**
 * Print out URI to IO buffer.
 */
sel_err_t law_uri_bprint(struct pgc_buf *buffer, struct law_uri *uri);

/**
 * Populate the URI from an AST.
 */
struct law_uri *law_uri_from_ast(
        struct law_uri *uri,
        struct pgc_ast_lst *ast);

/**
 * Parse the URI starting at the buffer's current offset.
 */
sel_err_t law_uri_parse(
        const struct pgc_par *parser,
        struct pgc_buf *buffer,
        struct pgc_stk *heap,
        struct law_uri *uri);

/** Parse the URI query string. */
sel_err_t law_uri_parse_query(
        char *query_string,
        struct pgc_stk *heap,
        struct law_uri_query **query);

/** Parse the URI path string. */
sel_err_t law_uri_parse_path(
        char *path_string,
        struct pgc_stk *heap,
        struct law_uri_path **path);

/** Capture scheme component. */
sel_err_t law_uri_cap_scheme(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture host component. */
sel_err_t law_uri_cap_host(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture port component. */
sel_err_t law_uri_cap_port(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture path component. */
sel_err_t law_uri_cap_path(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture query token. */
sel_err_t law_uri_cap_token(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

/** Capture query element. */
sel_err_t law_uri_cap_elem(
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

#endif