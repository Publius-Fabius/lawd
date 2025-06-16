#ifndef LAWD_DATA_H
#define LAWD_DATA_H

#include <stdint.h>

/** Generic Data */
typedef struct law_data {
        union {
                void *ptr;
                int fd;
                int8_t i8;
                uint8_t u8;
                int16_t i16;
                uint16_t u16;
                int32_t i32;
                uint32_t u32;
                uint64_t u64;
                int64_t i64;
                float f32;
                double f64;
        };
} law_data_t;

#endif