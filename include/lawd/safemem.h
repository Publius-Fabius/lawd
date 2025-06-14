#ifndef LAW_SAFEMEM_H
#define LAW_SAFEMEM_H

#include <stddef.h>

/** Guarded Memory */
typedef struct law_smem law_smem_t;

/**
 * Allocate 'length' bytes with protected 'guard'-N bytes on both sides.  
 * Attempting to read or write to the protected regions will result in a 
 * segmentation fault.
 * @param length The number of addressable bytes to allocate.
 * @param guard The length of the protected regions.
 * @return A new pgc_smem structure.
 */
law_smem_t *law_smem_create(
        const size_t length, 
        const size_t guard);

/**
 * Destroy the memory. 
 * @param mem The memory to destroy.
 */
int law_smem_destroy(law_smem_t *mem);

/**
 * Get the beginning of the addressable bytes. 
 * @param mem The memory.
 * @return The beginning of the addressable region.
 */
void *law_smem_address(law_smem_t *mem);

/**
 * Return the length of the addressable region.
 * @param mem The memory.
 * @return The length of the addressable region. 
 */
size_t law_smem_length(law_smem_t *mem);

#endif