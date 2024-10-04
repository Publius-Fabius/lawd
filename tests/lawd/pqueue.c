
#include "lawd/error.h"
#include "lawd/pqueue.h"
#include <stdlib.h>

void test_enq_deq()
{
        SEL_INFO();
        struct law_pqueue *q = law_pq_create();

        int64_t x, y, z, w, a = 1;

        law_pq_insert(q, 5, &x);
        law_pq_insert(q, 2, &y);
        law_pq_insert(q, 6, &a);
        law_pq_insert(q, 4, &z);
        law_pq_insert(q, 3, &w);

        int64_t *val, pri;

        SEL_TEST(law_pq_size(q) == 5);

        law_pq_peek(q, &pri, (void**)&val);
        SEL_TEST(val == &y);
        SEL_TEST(pri == 2);
        
        law_pq_pop(q);
        SEL_TEST(law_pq_size(q) == 4);
        law_pq_peek(q, &pri, (void**)&val);
        SEL_TEST(val == &w);
        SEL_TEST(pri == 3);

        law_pq_pop(q);
        SEL_TEST(law_pq_size(q) == 3);
        law_pq_peek(q, &pri, (void**)&val);
        SEL_TEST(val == &z);
        SEL_TEST(pri == 4);

        law_pq_pop(q);
        SEL_TEST(law_pq_size(q) == 2);
        law_pq_peek(q, &pri, (void**)&val);
        SEL_TEST(val == &x);
        SEL_TEST(pri == 5);

        law_pq_pop(q);
        SEL_TEST(law_pq_size(q) == 1);
        law_pq_peek(q, &pri, (void**)&val);
        SEL_TEST(val == &a);
        SEL_TEST(pri == 6);

        law_pq_pop(q);
        SEL_TEST(law_pq_size(q) == 0);
        law_pq_peek(q, &pri, (void**)&val);

        law_pq_destroy(q);
}

int main(int argc, char **args) 
{
        SEL_INFO();
        
        test_enq_deq();

        return EXIT_SUCCESS;
}