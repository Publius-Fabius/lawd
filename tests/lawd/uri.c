
#include "lawd/uri.h"
#include "pgenc/lang.h"
#include <string.h>

void test_sub_delims()
{
        SEL_INFO();

        /* sub_delims  = "!" | "$" | "&" | "'" | "(" | ")"
                         | "*" | "+" | "," | ";" | "=" */

        struct pgc_par *sub_delims = law_uri_parsers_sub_delims(
                export_law_uri_parsers());
        
        /* Test inclusion in set. */
        SEL_ASSERT(pgc_par_runs(sub_delims, "!", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "$", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "&", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "'", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "(", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, ")", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "*", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "+", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, ",", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, ";", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "=", NULL) > 0);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(sub_delims, "a", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "#", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "~", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(sub_delims, "0", NULL) < 0);
}

void test_gen_delims()
{
        SEL_INFO();

        /* gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@" */

        struct pgc_par *gen_delims = law_uri_parsers_gen_delims(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(gen_delims, ":", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "/", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "?", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "#", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "[", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "]", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "@", NULL) > 0);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(gen_delims, "a", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "-", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "~", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(gen_delims, "0", NULL) < 0);
}

void test_reserved()
{
        SEL_INFO();

        /* reserved = gen_delims | sub_delims */

        struct pgc_par *reserved = law_uri_parsers_reserved(
                export_law_uri_parsers());
        
        /* Test inclusion in set. */
        SEL_ASSERT(pgc_par_runs(reserved, "!", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "$", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "&", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "'", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "(", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, ")", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "*", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "+", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, ",", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, ";", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "=", NULL) > 0);

        SEL_ASSERT(pgc_par_runs(reserved, ":", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "/", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "?", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "#", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "[", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "]", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(reserved, "@", NULL) > 0);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(reserved, "a", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(reserved, "-", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(reserved, "~", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(reserved, "0", NULL) < 0);
}

void test_unreserved()
{
        SEL_INFO();

        /* unreserved = ALPHA | DIGIT | "-" | "." | "_" | "~" */

        struct pgc_par *unreserved = law_uri_parsers_unreserved(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(unreserved, "A", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "a", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "z", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "Z", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "0", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "9", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "-", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, ".", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "_", NULL) > 0);
        SEL_ASSERT(pgc_par_runs(unreserved, "~", NULL) > 0);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(unreserved, "[", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(unreserved, ")", NULL) < 0);
}

void test_scheme()
{
        SEL_INFO();

        /* scheme = ALPHA *( ALPHA | DIGIT | "+" | "-" | "." ) */

        struct pgc_par *scheme = law_uri_parsers_scheme(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(scheme, "H1+-.~~", NULL) == 5);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(scheme, "[", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(scheme, "+", NULL) < 0);
}

void test_dec_octet()
{
        SEL_INFO();

        /* dec-octet     = DIGIT                 ; 0-9
         *               | %x31-39 DIGIT         ; 10-99
         *               | "1" 2DIGIT            ; 100-199
         *               | "2" %x30-34 DIGIT     ; 200-249
         *               | "25" %x30-35          ; 250-255 */

        struct pgc_par *dec_octet = law_uri_parsers_dec_octet(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(dec_octet, "0a", NULL) == 1);
        SEL_ASSERT(pgc_par_runs(dec_octet, "10a", NULL) == 2);
        SEL_ASSERT(pgc_par_runs(dec_octet, "99a", NULL) == 2);
        SEL_ASSERT(pgc_par_runs(dec_octet, "100a", NULL) == 3);
        SEL_ASSERT(pgc_par_runs(dec_octet, "199a", NULL) == 3);
        SEL_ASSERT(pgc_par_runs(dec_octet, "200a", NULL) == 3);
        SEL_ASSERT(pgc_par_runs(dec_octet, "249a", NULL) == 3);
        SEL_ASSERT(pgc_par_runs(dec_octet, "250a", NULL) == 3);
        SEL_ASSERT(pgc_par_runs(dec_octet, "255a", NULL) == 3);
        SEL_ASSERT(pgc_par_runs(dec_octet, "256a", NULL) == 2);
        SEL_ASSERT(pgc_par_runs(dec_octet, "266a", NULL) == 2);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(dec_octet, "[", NULL) < 0);
}

void test_IPv4address()
{
        SEL_INFO();

        /* IPv4address = 
                dec-octet "." dec-octet "." dec-octet "." dec-octet */

        struct pgc_par *IPv4addr = law_uri_parsers_IPv4address(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(IPv4addr, "128.128.128.128", NULL) == 15);
        SEL_ASSERT(pgc_par_runs(IPv4addr, "255.255.255.255", NULL) == 15);
        SEL_ASSERT(pgc_par_runs(IPv4addr, "10.9.255.0", NULL) == 10);
        SEL_ASSERT(pgc_par_runs(IPv4addr, "10.9.255.256", NULL) == 11);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(IPv4addr, "255.255.256.255", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(IPv4addr, "010.9.255.0", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(IPv4addr, "10.09.255.0", NULL) < 0);
        SEL_ASSERT(pgc_par_runs(IPv4addr, "[", NULL) < 0);
}

void test_IPv6address()
{
        SEL_INFO();

 /* IPv6address */

        struct pgc_par *IPv6addr = law_uri_parsers_IPv6address(
                export_law_uri_parsers());

                /*

*/
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:0db8:85a3:0000:0000:8a2e:0370:7334", 
                NULL) == 39);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:db8:85a3:0:0:8a2e:370:7334", 
                NULL) == 31);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "::1", 
                NULL) == 3);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "::", 
                NULL) == 2);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:db8::", 
                NULL) == 10);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "::ffff:192.0.2.1", 
                NULL) == 16);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "::192.0.2.1", 
                NULL) == 11);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "64:ff9b::192.0.2.33", 
                NULL) == 19);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:db8:3333:4444:5555:6666:7777:8888", 
                NULL) == 38);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "::11:22:33:44:55:66", 
                NULL) == 19);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:db8::1:0:0:1", 
                NULL) == 17);

/*

*/
        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "255.255.256.255", 
                NULL) < 0);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                ":2001:db8:85a3:0:0:8a2e:370:7334", 
                NULL) < 0);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001::db8::1234", 
                NULL) == 9);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:db8:85a3:0:0:8a2e:370:73341", 
                NULL) == 31);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "2001:0db8:85a3:0000:8a2e:0370:1234::7334", 
                NULL) == 36);
        SEL_ASSERT(pgc_par_runs(IPv6addr, 
                "1200:0000:AB00:1234:O000:2552:7777:1313", 
                NULL) < 0);
}

void test_reg_name()
{
        SEL_INFO();

        /* reg_name = *( unreserved | pct_encoded | sub_delims ) */

        struct pgc_par *reg_name = law_uri_parsers_reg_name(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(reg_name, "", NULL) == 0);
        SEL_ASSERT(pgc_par_runs(reg_name, "abc%20", NULL) == 6);
        SEL_ASSERT(pgc_par_runs(reg_name, "a!c%20ab", NULL) == 8);
        SEL_ASSERT(pgc_par_runs(reg_name, "a!~%20a?b", NULL) == 7);

        /* Test exclusion from set. */
        SEL_ASSERT(pgc_par_runs(reg_name, ":", NULL) == 0);
        SEL_ASSERT(pgc_par_runs(reg_name, "@", NULL) == 0);
}

void test_authority()
{
        SEL_INFO();

        /* host = IP_literal | IPv4address | reg_name
         * port = *DIGIT
         * authority = host [ ":" port ] */

        struct pgc_par *authority = law_uri_parsers_authority(
                export_law_uri_parsers());

        SEL_ASSERT(pgc_par_runs(authority, "[::1]:500", NULL) == 9);
        SEL_ASSERT(pgc_par_runs(authority, "abc%20:70/", NULL) == 9);
        SEL_ASSERT(pgc_par_runs(authority, "a!c%20ab?", NULL) == 8);
        SEL_ASSERT(pgc_par_runs(authority, "a!~%20a:5?b", NULL) == 9);

        /* Test exclusion from set. */
        // SEL_ASSERT(pgc_par_runs(authority, "", NULL) == 0);
}

enum pgc_err test_lang_parse(
        struct pgc_par *parser, 
        char *text,
        struct pgc_ast_lst **result)
{
        static char bytes[0x2000];
        struct pgc_stk stk;
        pgc_stk_init(&stk, 0x2000, bytes);

        struct pgc_buf buf;
        size_t text_len = strlen(text);
        pgc_buf_init(&buf, text, text_len, text_len);
        
        return pgc_lang_parse(parser, &buf, &stk, result);
}

void test_cap_authority()
{
        SEL_INFO();

        struct pgc_par *auth = law_uri_parsers_cap_authority(
                export_law_uri_parsers());

        struct pgc_ast_lst *result;
        struct pgc_ast *value;

        SEL_ASSERT(test_lang_parse(auth, "abc:21", &result) == PGC_ERR_OK);
        SEL_ASSERT(pgc_ast_len(result) == 2);
        value = result->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_HOST);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "abc"));
        value = pgc_ast_at(result, 1)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_PORT);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "21"));
}

void test_cap_absolute_URI()
{
        SEL_INFO();

        struct pgc_par *uri = law_uri_parsers_cap_absolute_URI(
                export_law_uri_parsers());

        struct pgc_ast_lst *result;
        struct pgc_ast *value;

        SEL_ASSERT(test_lang_parse(uri, 
                "http://hello.com:80/my/path?query", 
                &result) == PGC_ERR_OK);

        SEL_ASSERT(pgc_ast_len(result) == 5);
        value = pgc_ast_at(result, 0)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_SCHEME);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "http"));
        value = pgc_ast_at(result, 1)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_HOST);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "hello.com"));
        value = pgc_ast_at(result, 2)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_PORT);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "80"));
        value = pgc_ast_at(result, 3)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_PATH);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "/my/path"));
        value = pgc_ast_at(result, 4)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_QUERY);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "query"));
}

void test_cap_origin_URI()
{
        SEL_INFO();

        struct pgc_par *uri = law_uri_parsers_cap_origin_URI(
                export_law_uri_parsers());

        struct pgc_ast_lst *result;
        struct pgc_ast *value;

        SEL_ASSERT(test_lang_parse(uri, 
                "/my/path?query", 
                &result) == PGC_ERR_OK);

        SEL_ASSERT(pgc_ast_len(result) == 2);
        value = pgc_ast_at(result, 0)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_PATH);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "/my/path"));
        value = pgc_ast_at(result, 1)->val;
        SEL_ASSERT(pgc_syn_typeof(value) == LAW_URI_QUERY);
        SEL_ASSERT(!strcmp(pgc_ast_tostr(value), "query"));
}

enum pgc_err test_uri_parses(
        struct pgc_par *parser, 
        char *text,
        struct law_uri *uri)
{
        static char bytes[0x2000];
        struct pgc_stk stk;
        pgc_stk_init(&stk, 0x2000, bytes);

        struct pgc_buf buf;
        size_t text_len = strlen(text);
        pgc_buf_init(&buf, text, text_len, text_len);
        
        return law_uri_parse(parser, &buf, &stk, uri);
}

void test_uri_parse()
{
        SEL_INFO();

        struct pgc_par *uri = law_uri_parsers_cap_absolute_URI(
                export_law_uri_parsers());

        struct law_uri result;

        SEL_ASSERT(test_uri_parses(uri, 
                "http://hello.com:80/my/path?query", 
                &result) == PGC_ERR_OK);

        SEL_ASSERT(!strcmp(result.scheme, "http"));
        SEL_ASSERT(!strcmp(result.host, "hello.com"));
        SEL_ASSERT(!strcmp(result.port, "80"));
        SEL_ASSERT(!strcmp(result.path, "/my/path"));
        SEL_ASSERT(!strcmp(result.query, "query"));
}

int main(int argc, char ** args)
{
        SEL_INFO();
        test_sub_delims();
        test_gen_delims();
        test_reserved();
        test_unreserved();
        test_scheme();
        test_dec_octet();
        test_IPv4address();
        test_IPv6address();
        test_reg_name();
        test_authority();
        test_cap_authority();
        test_cap_absolute_URI();
        test_cap_origin_URI();
        test_uri_parse();
}