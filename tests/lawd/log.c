
#include "lawd/log.h"
#include <stdlib.h>

void test_log_error()
{
        law_log_err(stderr, "tests", "my message");
}

int main(int argc, char **args) 
{
        test_log_error();
}