#ifndef LAW_SAFEMEM_H
#define LAW_SAFEMEM_H

#include <stddef.h>

/** Guarded Memory */
struct law_smem;

/**
 * Allocate 'length' bytes with protected 'guard'-N bytes on both sides.  
 * Attempting to read or write to the protected regions will result in a 
 * segmentation fault.
 * @param length The number of addressable bytes to allocate.
 * @param guard The length of the protected regions.
 * @return A new pgc_smem structure.
 */
extern struct law_smem *law_smem_create(
        const size_t length, 
        const size_t guard);

/**
 * Destroy the memory. 
 * @param mem The memory to destroy.
 */
extern int law_smem_destroy(struct law_smem *mem);

/**
 * Get the beginning of the addressable bytes. 
 * @param mem The memory.
 * @return The beginning of the addressable region.
 */
extern void *law_smem_address(struct law_smem *mem);

/**
 * Return the length of the addressable region.
 * @param mem The memory.
 * @return The length of the addressable region. 
 */
extern size_t law_smem_length(struct law_smem *mem);

#endif