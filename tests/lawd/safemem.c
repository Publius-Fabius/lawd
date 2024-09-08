
#include "selc/error.h"
#include "lawd/safemem.h"

void test_create()
{
        SEL_INFO();
        struct law_smem *mem = law_smem_create(4096, 4096);
        SEL_TEST(mem);
        law_smem_destroy(mem);
}

void test_address()
{
        SEL_INFO();
        struct law_smem *mem = law_smem_create(4096, 4096);
        SEL_TEST(mem);
        char *bytes = law_smem_address(mem);
        bytes[0] = 'a';
        bytes[1] = 'b';
        bytes[2] = 'c';
        law_smem_destroy(mem);
}

int main(int argc, char **args) 
{
        SEL_INFO();
        test_create();
        test_address();
}