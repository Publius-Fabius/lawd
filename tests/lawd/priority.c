
#include "lawd/priority.h"
#include "selc/error.h"
#include <stdio.h>
#include <stdlib.h>

int shuffle_cmp(const void *a, const void *b)
{
        return (rand() % 3) - 1; 
}

void shuffle_i64(int64_t *array, size_t n)
{
        qsort(array, n, sizeof(int64_t), shuffle_cmp);
}

int compare_i64(struct law_data a, struct law_data b)
{
        return a.i64 < b.i64;
}

int64_t *make_deck(int64_t min, int64_t max)
{
        SEL_ASSERT(min <= max);
        const size_t size = (size_t)(max - min);
        int64_t *deck = malloc(sizeof(int64_t) * size);
        for(int64_t x = min; x < size; ++x)
                deck[x] = x;
        shuffle_i64(deck, size);
        shuffle_i64(deck, size);
        return deck;
}

void test_insert() 
{
        SEL_INFO();
        struct law_pq *pq = law_pq_create(compare_i64);

        int64_t *deck = make_deck(0, 100);
        for(int64_t x = 0; x < 100; ++x)
                printf("%li ", deck[x]);
        printf("\n");

        for(int64_t x = 0; x < 100; ++x) {
                SEL_ASSERT(law_pq_insert(
                        pq, (struct law_data){ .i64 = deck[x] }, 0, NULL) == 1);
        }
        
        SEL_ASSERT(law_pq_size(pq) == 100);

        for(int64_t x = 0; x < 100; ++x) {
                struct law_data pri;
                SEL_ASSERT(law_pq_pop(pq, &pri, NULL, NULL) == 1);
                SEL_ASSERT(pri.i64 == x);
                printf("%li ", pri.i64);
        }

        SEL_ASSERT(law_pq_pop(pq, NULL, NULL, NULL) == 0);
        SEL_ASSERT(law_pq_size(pq) == 0);
        
        free(deck);
        law_pq_destroy(pq);
}

int main(int argc, char **args)
{
        SEL_INFO();
        test_insert();
}