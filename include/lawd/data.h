#ifndef LAWD_DATA_H
#define LAWD_DATA_H

#include <stdint.h>

/** Generic Data */
struct law_data {
        union {
                void *ptr;
                int fd;
                uint64_t u64;
                uint32_t u32;
                int64_t i64;
                int32_t i32;
                float f32;
                double f64;
        };
};

typedef struct law_data law_args_t[8];


#endif