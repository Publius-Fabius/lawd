
#include "lawd/safemem.h"
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>

struct law_smem {
        size_t guard;
        size_t length;
        uint8_t *address;
};

struct law_smem *law_smem_create(const size_t length, const size_t guard)
{
        struct law_smem *mem = malloc(sizeof(struct law_smem));

        if(!mem) {
                return NULL;
        }

        const int devz = open("/dev/zero", O_RDWR);
        if(devz < 0) {
                goto FAILURE;
        }

        /* Attempt to map the memory region. */
        uint8_t *address = mmap(
                NULL,
                guard + length + guard,
                PROT_NONE,
                MAP_PRIVATE,
                devz,
                0);

        /* Close /dev/zero no matter what. */
        close(devz);

        if(address == MAP_FAILED) {
                goto FAILURE;
        }

        /* Attempt to gain the appropriate READ/WRITE access. */
        if(mprotect(address + guard, length, PROT_READ | PROT_WRITE) < 0) {
                goto FAILURE;
        }

        mem->length = length;
        mem->guard = guard;
        mem->address = address;
        return mem;
        
        FAILURE:

        free(mem);
        return NULL;
}

int law_smem_destroy(struct law_smem *mem)
{
        if(!mem) {
                return 0;
        }
        const int err = munmap(
                mem->address, 
                mem->guard + mem->length + mem->guard);
        free(mem);
        return err;
}

void *law_smem_address(struct law_smem *mem)
{
        return mem->address + mem->guard;
}

size_t law_smem_length(struct law_smem *mem)
{
        return mem->length;
}