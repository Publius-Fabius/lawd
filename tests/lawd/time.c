
#include "lawd/time.h"
#include <assert.h>
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

void test_create_timer()
{
        law_timer_t *timer = law_timer_create(16);

        law_timer_destroy(timer);
}

int shuffle_cmp(const void *a, const void *b)
{
        return (rand() % 3) - 1; 
}

void shuffle(int *array, size_t n)
{
        qsort(array, n, sizeof(int), shuffle_cmp);
}

int *make_deck(int min, int max)
{
        assert(min <= max);
        const size_t size = (size_t)(max - min);
        int *deck = malloc(sizeof(int) * size);
        for(int x = min; x < size; ++x)
                deck[x] = x;
        shuffle(deck, size);
        shuffle(deck, size);
        return deck;
}

void test_insert()
{
        law_timer_t *timer = law_timer_create(16);

        assert(law_timer_size(timer) == 0);
        assert(law_timer_insert(timer, 7, 7, 7));
        assert(law_timer_size(timer) == 1);

        assert(law_timer_insert(timer, 4, 4, 4));
        assert(law_timer_size(timer) == 2);

        assert(law_timer_insert(timer, 3, 3, 3));
        assert(law_timer_size(timer) == 3);

        law_timer_destroy(timer);
}

void test_peek()
{
        law_timer_t *timer = law_timer_create(16);

        law_time_t expiry;
        law_vers_t vers;
        law_id_t id;

        assert(law_timer_insert(timer, 7, 8, 9));
        assert(law_timer_peek(timer, &expiry, &id, &vers));
        assert(expiry == 7 && id == 8 && vers == 9);

        assert(law_timer_insert(timer, 4, 5, 6));
        assert(law_timer_peek(timer, &expiry, &id, &vers));
        assert(expiry == 4 && id == 5 && vers == 6);

        assert(law_timer_insert(timer, 3, 4, 5));
        assert(law_timer_peek(timer, &expiry, &id, &vers));
        assert(expiry == 3 && id == 4 && vers == 5);

        assert(law_timer_insert(timer, 9, 10, 11));
        assert(law_timer_peek(timer, &expiry, &id, &vers));
        assert(expiry == 3 && id == 4 && vers == 5);

        law_timer_destroy(timer);
}

void test_pop()
{
        law_timer_t *timer = law_timer_create(16);

        law_time_t expiry;
        law_vers_t vers;
        law_id_t id;

        int *deck = make_deck(0, 500);

        for(int x = 0; x < 500; ++x) {
                int n = deck[x];
                assert(law_timer_insert(
                        timer, 
                        (law_time_t)n, 
                        (law_id_t)(n + 1), 
                        (law_vers_t)(n + 2)));
        }

        for(int x = 0; x < 500; ++x) {
                assert(law_timer_size(timer) == (500 - x));
                assert(law_timer_pop(timer, &expiry, &id, &vers));
                assert(expiry == x && id == x + 1 && vers == x + 2);
        }

        free(deck);

        law_timer_destroy(timer);
}

int main(int argc, char **args)
{
        test_millis();
        test_datetime();
        test_create_timer();
        test_insert();
        test_peek();
        test_pop();
        return EXIT_SUCCESS;
}