#include "lawd/http/parse.h"
#include "lawd/uri.h"
#include "pgenc/lang.h"
#include <stdlib.h>
#include <string.h>

extern const struct pgc_par law_htp_tchar;

void test_tchar()
{
        SEL_INFO();
        
        const struct pgc_par *p = &law_htp_tchar;

        /* tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
                / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
                / DIGIT / ALPHA */

        SEL_TEST(pgc_par_runs(p, "a", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "A", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "z", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "Z", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "0", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "9", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "!", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "#", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "$", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "%", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "'", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "*", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "+", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "-", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, ".", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "^", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "_", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "`", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "|", NULL) == 1);
        SEL_TEST(pgc_par_runs(p, "~", NULL) == 1);

        /* exclusion */
        SEL_TEST(pgc_par_runs(p, ":", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "<", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, ")", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "]", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "\r", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "\n", NULL) < 0);
}

extern const struct pgc_par law_htp_token;

void test_token()
{
        SEL_INFO();
        
        const struct pgc_par *p = &law_htp_token;

        /* tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
                / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
                / DIGIT / ALPHA 
           token = 1*tchar  */

        SEL_TEST(pgc_par_runs(p, "aA!#$:", NULL) == 5);
        SEL_TEST(pgc_par_runs(p, "%'*+-\r\n", NULL) == 5);
        SEL_TEST(pgc_par_runs(p, ".^_`|~(", NULL) == 6);
    
        /* exclusion */
        SEL_TEST(pgc_par_runs(p, ":", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "<", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, ")", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "]", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "\r", NULL) < 0);
        SEL_TEST(pgc_par_runs(p, "\n", NULL) < 0);
}


enum pgc_err test_lang_parse(
        const struct pgc_par *parser, 
        char *text,
        struct pgc_ast_lst **result)
{
        static char bytes[0x2000];
        struct pgc_stk stk;
        pgc_stk_init(&stk, bytes, 0x2000);

        struct pgc_buf buf;
        size_t text_len = strlen(text);
        pgc_buf_init(&buf, text, text_len, text_len);
        
        return pgc_lang_parse(parser, &buf, &stk, result);
}

extern const struct pgc_par law_htp_request_target;

void test_request_target()
{
        SEL_INFO();

        const struct pgc_par *p = &law_htp_request_target;

        struct pgc_ast_lst *list;
        struct pgc_ast *value;

        SEL_TEST(test_lang_parse(p, "*", &list) == PGC_ERR_OK);
        SEL_TEST(pgc_syn_typeof(list->val) == LAW_HTP_AUTHORITY_FORM);
        value = pgc_ast_tolst(list->val)->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_HOST);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "*"));

        SEL_TEST(test_lang_parse(p, "/a/bc?query", &list) == PGC_ERR_OK);
        SEL_TEST(pgc_syn_typeof(list->val) == LAW_HTP_ORIGIN_FORM);
        list = pgc_ast_tolst(list->val);
        SEL_TEST(pgc_ast_len(list) == 2);
        value = list->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_PATH);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "/a/bc"));
        value = list->nxt->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_QUERY);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "query"));

        SEL_TEST(test_lang_parse(p, 
                "http://a:80/bc?a=1&b=2", 
                &list) == PGC_ERR_OK);
        SEL_TEST(pgc_syn_typeof(list->val) == LAW_HTP_ABSOLUTE_FORM);
        list = pgc_ast_tolst(list->val);
        SEL_TEST(pgc_ast_len(list) == 5);
        value = pgc_ast_at(list, 0)->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_SCHEME);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "http"));
        value = pgc_ast_at(list, 1)->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_HOST);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "a"));
        value = pgc_ast_at(list, 2)->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_PORT);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "80"));
        value = pgc_ast_at(list, 3)->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_PATH);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "/bc"));
        value = pgc_ast_at(list, 4)->val;
        SEL_TEST(pgc_syn_typeof(value) == LAW_URI_QUERY);
        SEL_TEST(!strcmp(pgc_ast_tostr(value), "a=1&b=2"));
}

extern const struct pgc_par law_htp_request_head;

void test_request_head()
{
        SEL_INFO();

        const struct pgc_par *p = &law_htp_request_head;

        struct pgc_ast_lst *list;

        SEL_TEST(test_lang_parse(p, 
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:109.0)\r\n"
                "Accept: text/html,application/xhtml+xml\r\n"
                "Sec-Fetch-User: ?1\r\n"
                "\r\n", 
                &list) == PGC_ERR_OK);
        SEL_ASSERT(pgc_ast_len(list) == 7);
        SEL_ASSERT(!strcmp("GET", pgc_ast_tostr(pgc_ast_at(list, 0)->val)));
        SEL_ASSERT(!strcmp("HTTP/1.1", pgc_ast_tostr(pgc_ast_at(list, 2)->val)));
        
}

int main(int argc, char **argv)
{
        SEL_INFO();
        test_tchar();
        test_token();
        test_request_target();
        test_request_head();
}