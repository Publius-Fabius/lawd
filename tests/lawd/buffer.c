
#include "lawd/buffer.h"
#include "lawd/error.h"
#include <unistd.h>
#include <fcntl.h>

void test_read()
{
        uint8_t bytes[8];

        int fds[2];
        pipe(fds);
        
        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        struct pgc_buf buf;
        pgc_buf_init(&buf, bytes, 8, 0);

        write(fds[1], "test", 4);
        SEL_TEST(law_buf_read(&buf, fds[0], 4) == 4);
        SEL_TEST(pgc_buf_cmp(&buf, "test", 4) == PGC_ERR_OK);

        write(fds[1], "hello", 5);
        SEL_TEST(law_buf_read(&buf, fds[0], 6) == 5);
        SEL_TEST(pgc_buf_cmp(&buf, "hello", 5) == PGC_ERR_OK);

        write(fds[1], "test", 4);
        SEL_TEST(law_buf_read(&buf, fds[0], 4) == 4);
        SEL_TEST(pgc_buf_cmp(&buf, "test", 4) == PGC_ERR_OK);

        SEL_TEST(law_buf_read(&buf, fds[0], 1) == LAW_ERR_WANTR);

        close(fds[1]);

        SEL_TEST(law_buf_read(&buf, fds[0], 1) == LAW_ERR_EOF);

        close(fds[0]);

        // This triggers valgrind, but it is a success.
        // SEL_TEST(law_buf_read(&buf, fds[0], 1) == LAW_ERR_SYS);
}

void test_write()
{
        uint8_t bytes[8];

        int fds[2];
        pipe(fds);
        
        int flags = fcntl(fds[0], F_GETFL);
        fcntl(fds[0], F_SETFL, O_NONBLOCK | flags);

        flags = fcntl(fds[1], F_GETFL);
        fcntl(fds[1], F_SETFL, O_NONBLOCK | flags);

        struct pgc_buf buf;
        pgc_buf_init(&buf, bytes, 8, 0);

        char str[8] = { 0 };

        pgc_buf_put(&buf, "test", 4);
        SEL_TEST(law_buf_write(&buf, fds[1], 4) == 4);

        read(fds[0], str, 4);

        SEL_TEST(strcmp(str, "test") == 0);

        close(fds[1]);
        close(fds[0]);
}

int main(int argc, char **args)
{
        SEL_INFO();

        test_read();
        test_write();
}