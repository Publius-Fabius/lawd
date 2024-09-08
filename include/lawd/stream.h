#ifndef LAW_READER_H
#define LAW_READER_H

#include "pgenc/buffer.h"
#include "lawd/error.h"
#include "stddef.h"

/** Reader */
struct law_reader;

/** Get a buffer mapped to the reader's readable bytes. */
struct pgc_buf *law_reader_buffer(
        struct law_reader *reader);

/** Flush nbytes bytes. */
enum law_err law_reader_flush(
        struct law_reader *reader, 
        const size_t nbytes);

/** Get the reader's state. */
enum law_err law_reader_state(
        struct law_reader *reader);

/** Writer */
struct law_writer;

/** Get a buffer mapped to the writer's writable bytes. */
struct pgc_buf *law_writer_buffer(
        struct law_writer *writer);

/** Flush nbytes bytes. */
enum law_err law_writer_flush(
        struct law_writer *reader, 
        const size_t nbytes);

/** Get the writer's state. */
enum law_err law_writer_state(
        struct law_writer *reader);

#endif