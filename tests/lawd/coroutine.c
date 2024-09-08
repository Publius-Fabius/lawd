
#include "lawd/coroutine.h"
#include "lawd/error.h"

int test_callback_1(
        struct law_cor *caller, 
        struct law_cor *callee, 
        void *state)
{
        int *value = state;

        SEL_TEST(*value == 0xABCDEF);

        return ~*value;
}

void test_call()
{
        SEL_INFO();

        struct law_cor *caller = law_cor_create();
        struct law_cor *callee = law_cor_create();
        struct law_smem *stack = law_smem_create(4096, 4096);
        
        int state = 0xABCDEF;

        int result = law_cor_call(
                caller,
                callee,
                stack,
                test_callback_1,
                &state);

        SEL_TEST(result == ~state);
        
        law_smem_destroy(stack);
        law_cor_destroy(callee);
        law_cor_destroy(caller);
}

int test_callback_2(
        struct law_cor *caller, 
        struct law_cor *callee, 
        void *state)
{
        int *value = state;

        SEL_TEST(*value == 0xABCDEF);

        law_cor_yield(caller, callee, *value + 0xDAB);

        return ~*value;
}

void test_yield()
{
        SEL_INFO();

        struct law_cor *caller = law_cor_create();
        struct law_cor *callee = law_cor_create();
        struct law_smem *stack = law_smem_create(4096, 4096);
        
        int state = 0xABCDEF;

        int result = law_cor_call(
                caller,
                callee,
                stack,
                test_callback_2,
                &state);

        SEL_TEST(result == state + 0xDAB);
        
        law_smem_destroy(stack);
        law_cor_destroy(callee);
        law_cor_destroy(caller);
}

int test_callback_3(
        struct law_cor *caller, 
        struct law_cor *callee, 
        void *state)
{
        volatile int value = *((int*)state);
        volatile int yld_value = value + 0xDAB;
        volatile int ret_value = ~value;

        /* Did we get the correct state? */
        SEL_TEST(value == 0xABCDEF);

        int resume = law_cor_yield(caller, callee, yld_value);

        /* Did the resume signal pass through correctly? */
        SEL_TEST(resume = 0x1111);

        /* Test for stack mangling. */
        SEL_TEST(yld_value == 0xABCDEF + 0xDAB);
        SEL_TEST(ret_value == ~0xABCDEF);
        SEL_TEST(value == 0xABCDEF);

        return ret_value;
}

void test_resume()
{
        SEL_INFO();

        struct law_cor *caller = law_cor_create();
        struct law_cor *callee = law_cor_create();
        struct law_smem *stack = law_smem_create(4096, 4096);
        
        int state = 0xABCDEF;

        int result = law_cor_call(
                caller,
                callee,
                stack,
                test_callback_3,
                &state);

        SEL_TEST(result == state + 0xDAB);
        
        result = law_cor_resume(caller, callee, 0x1111);

        SEL_TEST(result == ~state);

        law_smem_destroy(stack);
        law_cor_destroy(callee);
        law_cor_destroy(caller);
}

int main(int argc, char **args) 
{
        SEL_INFO();
        test_call();
        test_yield();
        test_resume();
}