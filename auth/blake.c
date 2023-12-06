#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reference_impl.h"

static uint32_t base2_IV[8] = {
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
};

static size_t MSG_PERMUTATION[16] = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

inline static uint32_t rotate_right(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

// The mixing function, G, which mixes either a column or a diagonal.

inline static void g(
    uint32_t state[16], size_t a, size_t b, size_t c, size_t d, uint32_t mx, uint32_t my) {
    state[a] = state[a] + state[b] + mx;
    state[d] = rotate_right(state[d] ^ state[a], 16);
    state[c] = state[c] + state[d];
    state[b] = rotate_right(state[b] ^ state[c], 12);
    state[a] = state[a] + state[b] + my;
    state[d] = rotate_right(state[d] ^ state[a], 8);
    state[c] = state[c] + state[d];
    state[b] = rotate_right(state[b] ^ state[c], 7);
}

inline static void round_function(uint32_t state[16], uint32_t m[16]) {
    // Mix the columns.
    g(state, 0, 4, 8, 12, m[0], m[1]);
    g(state, 1, 5, 9, 13, m[2], m[3]);
    g(state, 2, 6, 10, 14, m[4], m[5]);
    g(state, 3, 7, 11, 15, m[6], m[7]);
    // Mix the diagonals.
    g(state, 0, 5, 10, 15, m[8], m[9]);
    g(state, 1, 6, 11, 12, m[10], m[11]);
    g(state, 2, 7, 8, 13, m[12], m[13]);
    g(state, 3, 4, 9, 14, m[14], m[15]);
}

inline static void permute(uint32_t m[16]) {
    uint32_t permuted[16];
    for (size_t i = 0; i < 16; i++) { permuted[i] = m[MSG_PERMUTATION[i]]; }
    memcpy(m, permuted, sizeof(permuted));
}
inline static void compress(const uint32_t chaining_value[8], const uint32_t block_words[16],
    uint64_t counter, uint32_t block_len, uint32_t flags, uint32_t out[16]) {
    uint32_t state[16] = {
        chaining_value[0],
        chaining_value[1],
        chaining_value[2],
        chaining_value[3],
        chaining_value[4],
        chaining_value[5],
        chaining_value[6],
        chaining_value[7],
        base2_IV[0],
        base2_IV[1],
        base2_IV[2],
        base2_IV[3],
        (uint32_t)counter,
        (uint32_t)(counter >> 32),
        block_len,
        flags,
    };
    uint32_t block[16];
    memcpy(block, block_words, sizeof(block));

    round_function(state, block);  // round 1
    permute(block);
    round_function(state, block);  // round 2
    permute(block);
    round_function(state, block);  // round 3
    permute(block);
    round_function(state, block);  // round 4
    permute(block);
    round_function(state, block);  // round 5
    permute(block);
    round_function(state, block);  // round 6
    permute(block);
    round_function(state, block);  // round 7

    for (size_t i = 0; i < 8; i++) {
        state[i] ^= state[i + 8];
        state[i + 8] ^= chaining_value[i];
    }
    memcpy(out, state, sizeof(state));

#ifdef DEBUG
    printf("Ch val:\n    ");
    for (size_t i = 0; i < 8; i++) printf("%08x ", chaining_value[i]);
    printf("\n");
    printf("Block: %d \n", block_len);
    for (size_t i = 0; i < 8; i++) printf("%08x ", block_words[i]);
    printf("\n");
    for (size_t i = 8; i < 16; i++) printf("%08x ", block_words[i]);
    printf("\n");
    printf("flags: %08x, counter %lld\n", flags, counter);
    // printf("IV: ");
    // for (size_t i = 0; i < 4; i++) printf("%08x ", IV[i]);
    // printf("\n");
    printf("Out:   ");
    for (size_t i = 0; i < 8; i++) printf("%08x ", out[i]);
    printf("\n-------\n");
#endif
}

inline static void words_from_little_endian_bytes(
    const void *bytes, size_t bytes_len, uint32_t *out) {
    assert(bytes_len % 4 == 0);
    const uint8_t *u8_ptr = (const uint8_t *)bytes;
    for (size_t i = 0; i < (bytes_len / 4); i++) {
        out[i] = ((uint32_t)(*u8_ptr++));
        out[i] += ((uint32_t)(*u8_ptr++)) << 8;
        out[i] += ((uint32_t)(*u8_ptr++)) << 16;
        out[i] += ((uint32_t)(*u8_ptr++)) << 24;
    }
}

inline static void output_chaining_value(const output *self, uint32_t out[8]) {
    uint32_t out16[16];
    compress(self->input_chaining_value, self->block_words, self->counter, self->block_len,
        self->flags, out16);
    memcpy(out, out16, 8 * 4);
}

inline static void output_root_bytes(const output *self, void *out, size_t out_len) {
    uint8_t *out_u8               = (uint8_t *)out;
    uint64_t output_block_counter = 0;

    while (out_len > 0) {
        uint32_t words[16];
        compress(self->input_chaining_value, self->block_words, output_block_counter,
            self->block_len, self->flags | ROOT, words);
        for (size_t word = 0; word < 16; word++) {
            for (int byte = 0; byte < 4; byte++) {
                if (out_len == 0) { return; }

                *out_u8 = (uint8_t)(words[word] >> (8 * byte));
                out_u8++;
                out_len--;
            }
        }
        output_block_counter++;
    }
}

inline static void chunk_state_init(_blake3_chunk_state *self, const uint32_t key_words[8],
    uint64_t chunk_counter, uint32_t flags) {
    memcpy(self->chaining_value, key_words, sizeof(self->chaining_value));
    self->chunk_counter = chunk_counter;
    memset(self->block, 0, sizeof(self->block));
    self->block_len         = 0;
    self->blocks_compressed = 0;
    self->flags             = flags;
}

inline static size_t chunk_state_len(const _blake3_chunk_state *self) {
    return BLAKE3_BLOCK_LEN * (size_t)self->blocks_compressed + (size_t)self->block_len;
}

inline static uint32_t chunk_state_start_flag(const _blake3_chunk_state *self) {
    if (self->blocks_compressed == 0) {
        return CHUNK_START;
    } else {
        return 0;
    }
}

/**
 * Updates the chunk state with the given input data.
 * If the block buffer is full, compress it and clear it.
 * More input is coming, so this compression is not CHUNK_END.
 * Copy input bytes into the block buffer.
 *
 * @param self The chunk state to update.
 * @param input The input data to update the chunk state with.
 * @param input_len The length of the input data.
 */

inline static void chunk_state_update(
    _blake3_chunk_state *self, const void *input, size_t input_len) {
    const uint8_t *input_u8 = (const uint8_t *)input;
    while (input_len > 0) {
        // If the block buffer is full, compress it and clear it. More input is
        // coming, so this compression is not CHUNK_END.
        if (self->block_len == BLAKE3_BLOCK_LEN) {
            // init an array of 16 32-bit words
            uint32_t block_words[16];

            // convert the block buffer to an array of 16 32-bit words
            words_from_little_endian_bytes(self->block, BLAKE3_BLOCK_LEN, block_words);

            // init an array of 16 32-bit words to store the output
            uint32_t out16[16];

            // run the compress function with
            compress(self->chaining_value, block_words, self->chunk_counter, BLAKE3_BLOCK_LEN,
                self->flags | chunk_state_start_flag(self), out16);
            memcpy(self->chaining_value, out16, sizeof(self->chaining_value));
            // printf("%2d ch val: ", tmp++);
            // for (size_t i = 0; i < 8; i++) printf("%08x ", self->chaining_value[i]);
            // printf("\n");
            self->blocks_compressed++;
            memset(self->block, 0, sizeof(self->block));
            self->block_len = 0;
        }

        // Copy input bytes into the block buffer.
        size_t want = BLAKE3_BLOCK_LEN - (size_t)self->block_len;
        size_t take = want;
        if (input_len < want) { take = input_len; }
        memcpy(&self->block[(size_t)self->block_len], input_u8, take);
        self->block_len += (uint8_t)take;
        input_u8 += take;
        input_len -= take;
    }
}

inline static output chunk_state_output(const _blake3_chunk_state *self) {
    output ret;
    memcpy(ret.input_chaining_value, self->chaining_value, sizeof(ret.input_chaining_value));
    words_from_little_endian_bytes(self->block, sizeof(self->block), ret.block_words);
    ret.counter   = self->chunk_counter;
    ret.block_len = (uint32_t)self->block_len;
    ret.flags     = self->flags | chunk_state_start_flag(self) | CHUNK_END;
    return ret;
}

inline static output parent_output(const uint32_t left_child_cv[8],
    const uint32_t right_child_cv[8], const uint32_t key_words[8], uint32_t flags) {
    output ret;
    memcpy(ret.input_chaining_value, key_words, sizeof(ret.input_chaining_value));
    memcpy(&ret.block_words[0], left_child_cv, 8 * 4);
    memcpy(&ret.block_words[8], right_child_cv, 8 * 4);
    ret.counter   = 0;                 // Always 0 for parent nodes.
    ret.block_len = BLAKE3_BLOCK_LEN;  // Always BLAKE3_BLOCK_LEN (64) for parent nodes.
    ret.flags     = PARENT | flags;
    return ret;
}

inline static void parent_cv(const uint32_t left_child_cv[8], const uint32_t right_child_cv[8],
    const uint32_t key_words[8], uint32_t flags, uint32_t out[8]) {
    output o = parent_output(left_child_cv, right_child_cv, key_words, flags);
    // We only write to `out` after we've read the inputs. That makes it safe
    // for `out` to alias an input, which we do below.
    output_chaining_value(&o, out);
}

inline static void hasher_init_internal(
    blake3_hasher *self, const uint32_t key_words[8], uint32_t flags) {
    chunk_state_init(&self->chunk_state, key_words, 0, flags);
    memcpy(self->key_words, key_words, sizeof(self->key_words));
    self->cv_stack_len = 0;
    self->flags        = flags;
}

void blake3_hasher_init(blake3_hasher *self) {
    hasher_init_internal(self, base2_IV, 0);
}

void blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]) {
    uint32_t key_words[8];
    words_from_little_endian_bytes(key, BLAKE3_KEY_LEN, key_words);
    hasher_init_internal(self, key_words, KEYED_HASH);
}

void blake3_hasher_init_derive_key(blake3_hasher *self, const char *context) {

    blake3_hasher context_hasher;
    hasher_init_internal(&context_hasher, base2_IV, DERIVE_KEY_CONTEXT);
    blake3_hasher_update(&context_hasher, context, strlen(context));
    uint8_t context_key[BLAKE3_KEY_LEN];
    blake3_hasher_finalize(&context_hasher, context_key, BLAKE3_KEY_LEN);
    uint32_t context_key_words[8];
    words_from_little_endian_bytes(context_key, BLAKE3_KEY_LEN, context_key_words);
    hasher_init_internal(self, context_key_words, DERIVE_KEY_MATERIAL);
}

inline static void hasher_push_stack(blake3_hasher *self, const uint32_t cv[8]) {
    memcpy(&self->cv_stack[(size_t)self->cv_stack_len * 8], cv, 8 * 4);
    self->cv_stack_len++;
}

// Returns a pointer to the popped CV, which is valid until the next push.

inline static const uint32_t *hasher_pop_stack(blake3_hasher *self) {
    self->cv_stack_len--;
    return &self->cv_stack[(size_t)self->cv_stack_len * 8];
}

// Section 5.1.2 of the BLAKE3 spec explains this algorithm in more detail.
inline static void hasher_add_chunk_cv(
    blake3_hasher *self, uint32_t new_cv[8], uint64_t total_chunks) {

    while ((total_chunks & 1) == 0) {
        parent_cv(hasher_pop_stack(self), new_cv, self->key_words, self->flags, new_cv);
        total_chunks >>= 1;
    }
    hasher_push_stack(self, new_cv);
}

void blake3_hasher_update(blake3_hasher *self, const void *input, size_t input_len) {
    const uint8_t *input_u8 = (const uint8_t *)input;
    while (input_len > 0) {
        // If the current chunk is complete, finalize it and reset the chunk
        // state. More input is coming, so this chunk is not ROOT.
        if (chunk_state_len(&self->chunk_state) == BLAKE3_CHUNK_LEN) {
            output   chunk_output = chunk_state_output(&self->chunk_state);
            uint32_t chunk_cv[8];
            output_chaining_value(&chunk_output, chunk_cv);

            uint64_t total_chunks = self->chunk_state.chunk_counter + 1;
            hasher_add_chunk_cv(self, chunk_cv, total_chunks);
            chunk_state_init(&self->chunk_state, self->key_words, total_chunks, self->flags);
        }

        // Compress input bytes into the current chunk state.
        size_t want = BLAKE3_CHUNK_LEN - chunk_state_len(&self->chunk_state);
        size_t take = want;
        if (input_len < want) { take = input_len; }
        chunk_state_update(&self->chunk_state, input_u8, take);
        input_u8 += take;
        input_len -= take;
    }
}

// Finalize the hash and write any number of output bytes.

void blake3_hasher_finalize(const blake3_hasher *self, void *out, size_t out_len) {

    output current_output         = chunk_state_output(&self->chunk_state);
    size_t parent_nodes_remaining = (size_t)self->cv_stack_len;
    while (parent_nodes_remaining > 0) {
        parent_nodes_remaining--;
        uint32_t current_cv[8];
        output_chaining_value(&current_output, current_cv);
        current_output = parent_output(
            &self->cv_stack[parent_nodes_remaining * 8], current_cv, self->key_words, self->flags);
    }
    output_root_bytes(&current_output, out, out_len);
}

/**
 * Computes the BLAKE3 hash of the input stream and prints it as hexadecimal.
 *
 * @param has_key       A boolean indicating whether a key is provided.
 * @param key           A pointer to the key.
 * @param derive_key_context A pointer to the context for key derivation.
 * @param output_len    The length of the output hash in bytes.
 * @param input_stream  A pointer to the input stream.
 */
void blake3(bool has_key, uint8_t *key, const char *derive_key_context, size_t output_len,
    FILE *input_stream, uint8_t *output) {
    // check if file is opened correctly
    if (input_stream == NULL) {
        printf("\n[ERROR:] Could not open file\n");
        exit(1);
    }

    // Initialize the hasher.
    blake3_hasher hasher;
    if (has_key) {
        blake3_hasher_init_keyed(&hasher, key);
    } else if (derive_key_context != NULL) {
        blake3_hasher_init_derive_key(&hasher, derive_key_context);
    } else {
        blake3_hasher_init(&hasher);
    }

    // Hash standard input until we reach EOF.
    unsigned char buf[65536];
    while (1) {
        size_t n = fread(buf, 1, sizeof(buf), input_stream);
        if (n == 0) { break; }  // EOF (or possibly an error)
        blake3_hasher_update(&hasher, buf, n);
    }

    // Finalize the hash.

    assert(output != NULL);
    blake3_hasher_finalize(&hasher, output, output_len);

    // Print the hash as hexadecimal.
#ifdef ENABLE_PRINT
    for (size_t i = 0; i < output_len; i++) printf("%02x", output[i]);
    printf("\n");
#endif
    // free(output);
}

void test_blake3(bool has_key, uint8_t *key, const char *derive_key_context, size_t output_len,
    FILE *input_stream, uint8_t *output) {
    blake3(has_key, key, derive_key_context, output_len, input_stream, output);
}
