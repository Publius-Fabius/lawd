
#include "lawd/time.h"
#include <stdlib.h>
#include <stdio.h>

void test_millis()
{
        printf("%li \r\n", law_time_millis());
}

void test_datetime()
{
        struct law_time_dt_buf buf;
        printf("%s\r\n", law_time_datetime(&buf));
}

int main(int argc, char **args)
{
        test_millis();
        test_datetime();
        return EXIT_SUCCESS;
}