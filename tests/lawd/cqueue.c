
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

        void *v;

        SEL_TEST(law_cq_deq(q, &v));
        SEL_TEST(v == &x);
        SEL_TEST(law_cq_deq(q, &v));
        SEL_TEST(v == &y);
        SEL_TEST(law_cq_deq(q, &v));
        SEL_TEST(v == &z);
        SEL_TEST(!law_cq_deq(q, &v));

        law_cq_enq(q, &x);
        law_cq_enq(q, &y);
        law_cq_enq(q, &z);

        SEL_TEST(law_cq_deq(q, &v));
        SEL_TEST(v == &x);
        SEL_TEST(law_cq_deq(q, &v));
        SEL_TEST(v == &y);
        SEL_TEST(law_cq_deq(q, &v));
        SEL_TEST(v == &z);
        SEL_TEST(!law_cq_deq(q, &v));

        law_cq_destroy(q);
}

int main(int argc, char **args) 
{
        SEL_INFO();
        test_enq_deq();
}