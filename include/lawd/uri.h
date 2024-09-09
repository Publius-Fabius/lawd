
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

 * 
 *
 */

#ifndef LAWD_URI_H
#define LAWD_URI_H

#include "pgenc/parser.h"
#include "pgenc/stack.h"
#include "lawd/error.h"
#include <stddef.h>

/** URI Path */
struct law_uri_path;

/** Path Iterator */
struct law_uri_path_iter;

/** URI Query */
struct law_uri_query;

/** Query Iterator */
struct law_uri_query_iter;

/** (U)niform (R)esource (I)dentifier */
struct law_uri {
        char *scheme;
        char *host;
        char *port;
        char *path;
        char *query;
        char *fragment;
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
 * A callback for iterating through a path's segments.
 */
typedef sel_err_t (*law_uri_path_callback_t)(
        const char *segment,
        void *user_state);

/**
 * Iterate efficiently through the path's segments.
 */
sel_err_t law_uri_path_foreach(
        struct law_uri_path *path,
        law_uri_path_callback_t callback,
        void *user_state);

/**
 * Get an iterator for the path's segments.
 */
struct law_uri_path_iter *law_uri_path_segments(
        struct law_uri_path *path);

/**
 * Advance the iterator.
 */
int law_uri_path_next(
        struct law_uri_path_iter *iter,
        const char **value);

/**
 * End the iteration early, freeing the iterator's memory.
 */
void law_uri_path_stop(
        struct law_uri_path_iter *iter);

/**
 * Resolve path by removing dot segments.
 */
struct law_uri_path *law_uri_path_remove_dot_segs(
        struct law_uri_path *path);

/**
 * Get the number of query elements in the collection.
 */
size_t law_uri_query_count(
        struct law_uri_query *query);

/** 
 * Get the query value by name. 
 */
const char *law_uri_query_lookup(
        struct law_uri_query *query, 
        char *name);

/**
 * Callback type for iterating through URI queries.
 */
typedef sel_err_t (*law_uri_query_callback_t)(
        const char *name,
        const char *value,
        void *user_state);

/** 
 * Iterate through the URI query. 
 */
sel_err_t law_uri_query_foreach(
        struct law_uri_query *query,
        law_uri_query_callback_t callback,
        void *user_state);

/**
 * Get an iterator for the query's pairs.
 */
struct law_uri_query_iter *law_uri_query_pairs(
        struct law_uri_query *query);

/**
 * Get the next <name, value> query pair.
 */
int law_uri_query_next(
        struct law_uri_query_iter *iter,
        const char **name,
        const char **value);

/**
 * Stop the iteration early, freeing any memory.
 */
void law_uri_query_stop(
        struct law_uri_query_iter *iter);

/**
 * Parse the URI starting at the buffer's current offset.
 */
sel_err_t law_uri_parse(
        struct pgc_buf *buffer,
        struct pgc_stk *heap,
        struct law_uri **uri);

/** Parse the URI query string. */
sel_err_t law_uri_parse_query(
        const char *query_string,
        struct pgc_stk *heap,
        struct law_uri_query **query);

/** Parse the URI path string. */
sel_err_t law_uri_parse_path(
        const char *path_string,
        struct pgc_stk *heap,
        struct law_uri_path **path);

/** Capture scheme component. */
enum pgc_err law_uri_capscheme(
        struct pgc_buf *buffer,
        void *state,
        struct pgc_par *arg);

/** URI Parser Collection */
struct law_uri_parsers;

/** Get the static URI parser collection. */
struct law_uri_parsers *export_law_uri_parsers();

struct pgc_par *law_uri_parsers_sub_delims(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_gen_delims(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_reserved(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_unreserved(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_scheme(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_dec_octet(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_IPv4address(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_IPv6address(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_pct_encoded(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_reg_name(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_host(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_port(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_authority(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_path_absolute(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_path(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_query(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_URI(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_URI_reference(struct law_uri_parsers *pars);

struct pgc_par *law_uri_parsers_absolute_URI(struct law_uri_parsers *pars);

#endif