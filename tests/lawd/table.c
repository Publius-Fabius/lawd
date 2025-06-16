
#include "lawd/table.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

void test_create_destroy()
{
        law_table_t *table = law_table_create(16);

        law_table_destroy(table);
}

void test_insert()
{
        law_table_t *table = law_table_create(16);

        assert(law_table_size(table) == 0);
        assert(law_table_insert(table, 5, NULL) == PMT_HM_SUCCESS);
        assert(law_table_size(table) == 1);
        assert(law_table_insert(table, 1, NULL) == PMT_HM_SUCCESS);
        assert(law_table_size(table) == 2);
        assert(law_table_insert(table, 3, NULL) == PMT_HM_SUCCESS);
        assert(law_table_size(table) == 3);
        assert(law_table_insert(table, 4, NULL) == PMT_HM_SUCCESS);
        assert(law_table_size(table) == 4);
        assert(law_table_insert(table, 5, NULL) == PMT_HM_EXISTS);
        assert(law_table_size(table) == 4);

        law_table_destroy(table);
}

void test_lookup()
{
        law_table_t *table = law_table_create(16);

        law_id_t 
                a = 1,
                b = 2,
                c = 3,
                d = 4;

        assert(law_table_insert(table, a, &a) == PMT_HM_SUCCESS);
        assert(law_table_insert(table, b, &b) == PMT_HM_SUCCESS);
        assert(law_table_insert(table, c, &c) == PMT_HM_SUCCESS);
        assert(law_table_insert(table, d, &d) == PMT_HM_SUCCESS);

        assert(law_table_lookup(table, a) == &a);
        assert(law_table_lookup(table, b) == &b);
        assert(law_table_lookup(table, c) == &c);
        assert(law_table_lookup(table, d) == &d);

        law_table_destroy(table);
}

void test_remove()
{
        law_table_t *table = law_table_create(16);

        law_id_t 
                a = 1,
                b = 2,
                c = 3,
                d = 4;

        assert(law_table_insert(table, a, &a) == PMT_HM_SUCCESS);
        assert(law_table_insert(table, b, &b) == PMT_HM_SUCCESS);
        assert(law_table_insert(table, c, &c) == PMT_HM_SUCCESS);
        assert(law_table_insert(table, d, &d) == PMT_HM_SUCCESS);

        assert(law_table_remove(table, 40) == NULL);
        assert(law_table_remove(table, a) == &a);
        assert(law_table_size(table) == 3);
        assert(law_table_remove(table, b) == &b);
        assert(law_table_size(table) == 2);
        assert(law_table_remove(table, c) == &c);
        assert(law_table_size(table) == 1);
        assert(law_table_remove(table, d) == &d);
        assert(law_table_size(table) == 0);
        
        law_table_destroy(table);
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

void test_iterate()
{
        law_table_t *table = law_table_create(16);

        int *deck = make_deck(0, 500);

        for(int x = 0; x < 500; ++x) {
                assert(law_table_insert(table, (law_id_t)deck[x], NULL) 
                        == PMT_HM_SUCCESS);
        }

        pmt_hm_iter_t iter;
        law_table_entries(table, &iter);
        int visits[500] = { 0 };
        law_id_t id;

        while(law_table_next(&iter, &id, NULL)) {
                ++visits[id];
        }

        for(int x = 0; x < 500; ++x) {
                assert(visits[x] == 1);
        }

        free(deck);

        law_table_destroy(table);
}

void test_max_size()
{
        law_table_t *table = law_table_create(16);
        
        size_t capacity = law_table_capacity(table);

        assert(capacity == 16);

        size_t max_size = law_table_max_size(table);

        for(int x = 0; x < max_size; ++x) {
                law_table_insert(table, (law_id_t)x, NULL);
        }

        assert(max_size == law_table_size(table));
        assert(capacity == law_table_capacity(table));
 
        law_table_destroy(table);
}

int main(int argc, char **args)
{
        puts("testing - table.c");

        test_create_destroy();
        test_insert();
        test_lookup();
        test_remove();
        test_iterate();
        test_max_size();
}