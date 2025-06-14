#ifndef LAWD_TABLE_H
#define LAWD_TABLE_H

#include "lawd/id.h"
#include "pubmt/hash_map.h"
#include <stddef.h>

/** Id to Pointer Hash Table */
typedef struct law_table law_table_t;

/**
 * Create a new table.
 * 
 * RETURNS: A NULL return value means memory allocation failed.
 */
law_table_t *law_table_create(const size_t capacity);

/**
 * Destroy the table.
 */
void law_table_destroy(law_table_t *table);

/**
 * Insert entry into the table.
 * 
 * RETURNS:
 *      PMT_HM_EXISTS
 *      PMT_HM_RESIZE
 *      PMT_HM_SUCCESS
 */
int law_table_insert(law_table_t *table, law_id_t id, void *pointer);

/**
 * Lookup pointer with given id.
 * 
 * RETURNS: A return value of 'NULL' means the entry didn't exist. 
 */
void *law_table_lookup(law_table_t *table, law_id_t id);

/**
 * Remove the entry, returning its pointer.
 * 
 * RETURNS: A return value of 'NULL' means the entry didn't exist.
 */
void *law_table_remove(law_table_t *table, law_id_t id);

/**
 * Get an iterator to the beginning of the hash map.
 */
void law_table_entries(law_table_t *table, pmt_hm_iter_t *iter);

/**
 * Get the next node in the iteration.
 * 
 * RETURNS: A value of 'false' indicates the iteration has ended.
 */
bool law_table_next(pmt_hm_iter_t *iter, law_id_t *id, void **pointer);

/** 
 * Does the iterator have a next node?
 */
bool law_table_is_next(pmt_hm_iter_t *iter);

/**
 * Get table's size.
 */
size_t law_table_size(law_table_t *table);

/**
 * Get table's max size before a resize.
 */
size_t law_table_max_size(law_table_t *table);

#endif