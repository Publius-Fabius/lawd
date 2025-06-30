
#include "lawd/http_conn.h"
#include "lawd/private/http_conn.h"
#include <unistd.h>
#include <fcntl.h>

void test_read_data()
{
        SEL_INFO();

        uint8_t bytes[8] = { 0 };

        struct pgc_buf in;
        pgc_buf_init(&in, bytes, 8, 0);

        int fds[2];
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_htconn_t conn = { 
                .in = &in, 
                .security = LAW_HTC_UNSECURED, 
                .socket = fds[0],
                .ssl = NULL,
                .out = NULL };

        write(fds[1], "test", 4);
        SEL_TEST(law_htc_read_data(&conn) == 4);

        write(fds[1], "test", 4);
        SEL_TEST(law_htc_read_data(&conn) == 4);

        write(fds[1], "s", 1);
        SEL_TEST(law_htc_read_data(&conn) == LAW_ERR_OOB);

        SEL_TEST(pgc_buf_cmp(&in, "test", 4) == LAW_ERR_OK);
        SEL_TEST(pgc_buf_cmp(&in, "test", 4) == LAW_ERR_OK);

        SEL_TEST(law_htc_read_data(&conn) == 1);
        SEL_TEST(pgc_buf_cmp(&in, "s", 1) == LAW_ERR_OK);

        SEL_TEST(law_htc_read_data(&conn) == LAW_ERR_WANTR);

        close(fds[1]);

        SEL_TEST(law_htc_read_data(&conn) == LAW_ERR_EOF);

        close(fds[0]);
}

void test_write_data()
{
        SEL_INFO();

        uint8_t bytes[8] = { 0 };

        struct pgc_buf out;
        pgc_buf_init(&out, bytes, 8, 0);

        int fds[2];
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_htconn_t conn = { 
                .out = &out,
                .in = NULL,
                .security = LAW_HTC_UNSECURED, 
                .socket = fds[1],
                .ssl = NULL };

        char str[8] = { 0 };

        pgc_buf_put(&out, "test", 4);
        pgc_buf_put(&out, "s", 1);
        
        SEL_TEST(law_htc_write_data(&conn) == 5);

        read(fds[0], str, 5);
        SEL_TEST(strcmp(str, "tests") == 0);

        close(fds[0]);
        close(fds[1]);
}

void test_ensure_input()
{
        SEL_INFO();

        uint8_t bytes[8] = { 0 };

        struct pgc_buf in;
        pgc_buf_init(&in, bytes, 8, 0);

        int fds[2];
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_htconn_t conn = { 
                .in = &in, 
                .security = LAW_HTC_UNSECURED, 
                .socket = fds[0],
                .ssl = NULL,
                .out = NULL };

        SEL_TEST(law_htc_ensure_input(&conn, 1) == LAW_ERR_WANTR);
        SEL_TEST(law_htc_ensure_input(&conn, 9) == LAW_ERR_OOB);
        
        write(fds[1], "test", 4);

        SEL_TEST(law_htc_ensure_input(&conn, 4) == LAW_ERR_OK);
        SEL_TEST(law_htc_ensure_input(&conn, 5) == LAW_ERR_OOB);
        SEL_TEST(pgc_buf_cmp(&in, "test", 4) == LAW_ERR_OK);
        SEL_TEST(law_htc_ensure_input(&conn, 5) == LAW_ERR_WANTR);
        SEL_TEST(law_htc_ensure_input(&conn, 9) == LAW_ERR_OOB);

        close(fds[1]);

        SEL_TEST(law_htc_ensure_input(&conn, 1) == LAW_ERR_EOF);

        close(fds[0]);
}

void test_ensure_output()
{
        SEL_INFO();

        uint8_t bytes[8] = { 0 };

        struct pgc_buf out;
        pgc_buf_init(&out, bytes, 8, 0);

        int fds[2];
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_htconn_t conn = { 
                .out = &out, 
                .security = LAW_HTC_UNSECURED, 
                .socket = fds[1],
                .ssl = NULL,
                .in = NULL };

        pgc_buf_put(&out, "test", 4);

        SEL_TEST(law_htc_ensure_output(&conn, 5) == LAW_ERR_OK);
        SEL_TEST(law_htc_ensure_output(&conn, 9) == LAW_ERR_OOB);

        char str[8] = { 0 };

        read(fds[0], str, 4);

        SEL_TEST(strcmp(str, "test") == 0);

        for(int x = 0; x < 100000; ++x) {
                pgc_buf_put(&out, "xyzw", 4);
                law_htc_ensure_output(&conn, 4);
        }

        SEL_TEST(law_htc_ensure_output(&conn, 1) == LAW_ERR_WANTW);

        close(fds[1]);
        close(fds[0]);
}

void test_read_scan()
{
        SEL_INFO();

        uint8_t bytes[8] = { 0 };

        struct pgc_buf in;
        pgc_buf_init(&in, bytes, 8, 0);

        int fds[2];
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_htconn_t conn = { 
                .in = &in, 
                .security = LAW_HTC_UNSECURED, 
                .socket = fds[0],
                .ssl = NULL,
                .out = NULL };

        write(fds[1], "test;;ab", 8);

        SEL_TEST(law_htc_read_scan(&conn, 0, ";;", 2) == LAW_ERR_OK);
        SEL_TEST(pgc_buf_tell(&in) == 6);

        // [abcdefg;] -- OOB, offset = 7
        pgc_buf_init(&in, bytes, 8, 0);
        write(fds[1], "abcdefg;", 8);
        SEL_TEST(law_htc_read_scan(&conn, 0, ";;", 2) == LAW_ERR_OOB);
        SEL_TEST(pgc_buf_tell(&in) == 7);

        // [abcdef;] -- WANTR, offset = 6
        pgc_buf_init(&in, bytes, 8, 0);
        write(fds[1], "abcdef;", 7);
        SEL_TEST(law_htc_read_scan(&conn, 0, ";;", 2) == LAW_ERR_WANTR);
        SEL_TEST(pgc_buf_tell(&in) == 6);

        SEL_TEST(law_htc_read_scan(&conn, 0, ";;;;;;;;;", 9) == LAW_ERR_OOB);
        SEL_TEST(pgc_buf_tell(&in) == 6);

        close(fds[1]);

        pgc_buf_init(&in, bytes, 8, 0);
        SEL_TEST(law_htc_read_scan(&conn, 0, ";;", 2) == LAW_ERR_EOF);

        close(fds[0]);
}

void test_flush()
{
        SEL_INFO();

        uint8_t bytes[8] = { 0 };

        struct pgc_buf out;
        pgc_buf_init(&out, bytes, 8, 0);

        int fds[2];
        pipe(fds);

        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        law_htconn_t conn = { 
                .out = &out, 
                .security = LAW_HTC_UNSECURED, 
                .socket = fds[1],
                .ssl = NULL,
                .in = NULL };

        char str[8] = { 0 };

        pgc_buf_put(&out, "test", 4);

        SEL_TEST(law_htc_flush(&conn) ==  LAW_ERR_OK);

        read(fds[0], str, 4);

        SEL_TEST(strcmp(str, "test") == 0);

        close(fds[0]);
        close(fds[1]);
}

int main(int argc, char **args)
{
        SEL_INFO();
        test_read_data();
        test_write_data();
        test_ensure_input();
        test_ensure_output();
        test_read_scan();
        test_flush();
}