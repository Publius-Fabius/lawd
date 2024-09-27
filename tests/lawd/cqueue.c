
#include "lawd/error.h"
#include "lawd/cqueue.h"

void test_enq_deq()
{
        SEL_INFO();
        struct law_cqueue *q = law_cq_create();

        int x, y, z = 1;

        law_cq_enq(q, &x);
        law_cq_enq(q, &y);
        law_cq_enq(q, &z);

        SEL_TEST(&x == law_cq_deq(q));
        SEL_TEST(&y == law_cq_deq(q));
        SEL_TEST(&z == law_cq_deq(q));
        SEL_TEST(NULL == law_cq_deq(q));

        law_cq_enq(q, &x);
        law_cq_enq(q, &y);
        law_cq_enq(q, &z);

        SEL_TEST(&x == law_cq_deq(q));
        SEL_TEST(&y == law_cq_deq(q));
        SEL_TEST(&z == law_cq_deq(q));
        SEL_TEST(NULL == law_cq_deq(q));

        law_cq_destroy(q);
}

int main(int argc, char **args) 
{
        SEL_INFO();
        test_enq_deq();
}