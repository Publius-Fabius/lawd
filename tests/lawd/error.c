#include "lawd/error.h"
#include <string.h>

void test_builtins() 
{
        SEL_INFO();
        SEL_TEST(!strcmp(sel_lookup(SEL_ERR_OK), "SEL_ERR_OK"));
        SEL_TEST(!strcmp(sel_lookup(SEL_ERR_SYS), "SEL_ERR_SYS"));
}

void test_customs() 
{
        SEL_INFO();
        SEL_TEST(!strcmp(sel_lookup(LAW_ERR_AGAIN), "LAW_ERR_AGAIN"));
        SEL_TEST(!strcmp(sel_lookup(LAW_ERR_MODE), "LAW_ERR_MODE"));
}

int main(int argc, char **args) 
{
        SEL_INFO();
        sel_init();
        law_err_init();
        test_builtins();
        test_customs() ;
}