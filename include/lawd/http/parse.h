#ifndef LAWD_HTTP_PARSE_H
#define LAWD_HTTP_PARSE_H

#include "lawd/error.h"
#include "pgenc/parser.h"

/*
 *                                 RFC7230 
 *    Hypertext Transfer Protocol (HTTP/1.1): Message Syntax and Routing
 * 
 * tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
 *       / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
 *       / DIGIT / ALPHA
 * 
 * token = 1*tchar
 * 
 * method = token
 * 
 * origin_form = absolute_path [ "?" query ]
 * absolute_form = absolute_URI
 * authority_form = authority
 * asterisk_form = "*"
 * 
 * request_target = origin_form
 *                | absolute_form
 *                | authority_form
 *                | asterisk_form
 * 
 * HTTP_name = "HTTP" ; HTTP
 * HTTP_version = HTTP_name "/" DIGIT "." DIGIT
 *
 * request_line = method SP request_target SP HTTP_version CRLF
 * 
 * field_name = token
 * 
 * VCHAR = "any visible USASCII character"
 * 
 * SP = ' '
 * 
 * HTAB = '\t' 
 * 
 * OWS = *( SP | HTAB )
 * 
 * field_vchar = VCHAR
 * 
 * field_content  = field_vchar [ 1*( SP | HTAB ) field_vchar ]
 * 
 * field_value = *( field_content )
 * 
 * header_field = field_name ":" OWS field_value OWS
 * 
 * status-code = 3DIGIT 
 * 
 * reason-phrase = *( HTAB | SP | VCHAR )
 * 
 * status-line = HTTP-version SP status-code SP reason-phrase CRLF
 *
 * start-line = status-line | request-line
 * 
 * message_body = *OCTET
 * 
 * HTTP_message = start_line
 *              *( header_field CRLF )
 *              CRLF
 *              [ message_body ]
 */

/** HTTP Message Tags */
enum law_htp_tag {
        LAW_HTP_STATUS,                 /** Status */
        LAW_HTP_METHOD,                 /** Method */
        LAW_HTP_VERSION,                /** Version */
        LAW_HTP_FIELD,                  /** Header Field */
        LAW_HTP_FIELD_NAME,             /** Header Field Name */
        LAW_HTP_FIELD_VALUE,            /** Header Field Value */
        LAW_HTP_ORIGIN_FORM,            /** Origin URI Form */
        LAW_HTP_ABSOLUTE_FORM,          /** Absolute URI Form */
        LAW_HTP_AUTHORITY_FORM          /** Authority URI Form */
};

sel_err_t law_htp_cap_status(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_method(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_version(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_field_name(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_field_value(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_field(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_origin_form(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_absolute_form(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

sel_err_t law_htp_cap_authority_form(
        struct pgc_stk *stack,
        struct pgc_buf *buffer,
        void *state,
        const struct pgc_par *arg);

#endif