#ifndef _BLAKE3_REFERENCE_IMPL_H
#define _BLAKE3_REFERENCE_IMPL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#define CHUNK_START         1 << 0
#define CHUNK_END           1 << 1
#define PARENT              1 << 2
#define ROOT                1 << 3
#define KEYED_HASH          1 << 4
#define DERIVE_KEY_CONTEXT  1 << 5
#define DERIVE_KEY_MATERIAL 1 << 6
#define BLAKE3_OUT_LEN      32
#define BLAKE3_KEY_LEN      32
#define BLAKE3_BLOCK_LEN    64
#define BLAKE3_CHUNK_LEN    1024
#define SMALL_BLOCK_SIZE    1024

// #define M_LOG2E             1.44269504088896340736  // log2(e)
#define CHUNK_SIZE     1024
#define CHUNK_SIZE_LOG 10

// This struct is private.
typedef struct _blake3_chunk_state {
    uint32_t chaining_value[8];
    uint64_t chunk_counter;
    uint8_t  block[BLAKE3_BLOCK_LEN];
    uint8_t  block_len;
    uint8_t  blocks_compressed;
    uint32_t flags;
} _blake3_chunk_state;

// An incremental hasher that can accept any number of writes.
typedef struct blake3_hasher {
    _blake3_chunk_state chunk_state;
    uint32_t            key_words[8];
    uint32_t            cv_stack[8 * 54];  // Space for 54 subtree chaining values:
    uint8_t             cv_stack_len;      // 2^54 * CHUNK_LEN = 2^64
    uint32_t            flags;
} blake3_hasher;

// Each chunk or parent node can produce either an 8-word chaining value or, by
// setting the ROOT flag, any number of final output bytes. The Output struct
// captures the state just prior to choosing between those two possibilities.
typedef struct output {
    uint32_t input_chaining_value[8];
    uint32_t block_words[16];
    uint64_t counter;
    uint32_t block_len;
    uint32_t flags;
} output;

void blake3_hasher_init(blake3_hasher *self);
void blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]);
void blake3_hasher_init_derive_key(blake3_hasher *self, const char *context);
void blake3_hasher_update(blake3_hasher *self, const void *input, size_t input_len);
void blake3_hasher_finalize(const blake3_hasher *self, void *out, size_t out_len);

void blake3(bool has_key, uint8_t *key, const char *derive_key_context, size_t output_len,
    FILE *input_stream, uint8_t *output);
void blake3_rec(bool has_key, uint8_t *key, const char *derive_key_context, size_t output_len,
    FILE *input_stream, uint8_t *output);

void test_blake3(bool has_key, uint8_t *key, const char *derive_key_context, size_t output_len,
    FILE *input_stream, uint8_t *output);

void parallel_blake3_hasher_update(blake3_hasher *self, const uint8_t *input, size_t input_len);

#endif  // _BLAKE3_REFERENCE_IMPL_H
