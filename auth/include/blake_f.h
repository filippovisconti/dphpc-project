#ifndef _MYBLAKE3
#define _MYBLAKE3

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "omp.h"
#include "reference_impl.h"

#define CEIL_DIV(x, y) ((int)ceil((double)(x) / (y)))

#define MIN(x, y)      ((x) < (y) ? (x) : (y))
#define MAX(x, y)      ((x) < (y) ? (y) : (x))

#define PRINT_WITH_NEWLINE(str, n)                                                                 \
    do {                                                                                           \
        for (int i = 0; (str)[i] != '\0'; ++i) {                                                   \
            printf("%c", (str)[i]);                                                                \
            if ((i + 1) % (n) == 0) { printf("\n"); }                                              \
        }                                                                                          \
    } while (0)

#ifndef BENCHMARK
#define myprintf printf
#else
#define myprintf(...) (void)0
#endif

int get_num_chunks(char *filename);
int set_num_threads(int num_chunks);
int next_multiple_of_64(int n);

void words_from_little_endian_bytes(const void *bytes, size_t bytes_len, uint32_t *out);
void compress(const uint32_t chaining_value[8], const uint32_t block_words[16], uint64_t counter,
    uint32_t block_len, uint32_t flags, uint32_t out[16]);
void compute_chunk_chaining_values(uint32_t *int_IV, char *read_buffer, int num_blocks,
    uint64_t counter_t, int chunk, uint32_t chaining_value[8], uint32_t base_flags);

void write_output(uint32_t *int_IV, uint32_t message_words[16], uint32_t counter_t, uint32_t flags,
    uint8_t *output, size_t output_len);

void myblake(char *filename, uint8_t *output, size_t output_len, bool has_key, uint8_t *key,
    const char *derive_key_context, uint32_t initial_flags);
#endif
