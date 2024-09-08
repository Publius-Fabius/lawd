
#include "lawd/util.h"

void test_scan()
{
        SEL_INFO();

        struct pgc_buf buf;
        pgc_buf_init(&buf, "abcdefg", 7, 7);

        sel_err_t err = law_util_scan(&buf, "de", 2);
        SEL_TEST(err == SEL_ERR_OK);
        SEL_TEST(pgc_buf_tell(&buf) == 3);

        struct pgc_buf lens;
        SEL_TEST(pgc_buf_seek(&buf, 0) == PGC_ERR_OK);
        pgc_buf_lens(&lens, &buf, 4);

        err = law_util_scan(&lens, "de", 2);
        SEL_TEST(err == PGC_ERR_OOB);
        SEL_TEST(pgc_buf_tell(&lens) == 3);

        pgc_buf_lens(&lens, &buf, 5);
        err = law_util_scan(&lens, "cde", 3);
        SEL_TEST(err == SEL_ERR_OK);
        SEL_TEST(pgc_buf_tell(&lens) == 2);
}

int main(int argc, char **args)
{
        SEL_INFO();

        test_scan();
}