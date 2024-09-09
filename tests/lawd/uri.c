
#include "lawd/uri.h"

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
}