#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "omp.h"
#include "reference_impl.h"

uint32_t IV[8] = {
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
};

size_t MSG_PERMUTATION[16] = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

// inline long double log2(const long double x) {
//     return log(x) * M_LOG2E;
// }

/**
 * Rotates the bits of a 32-bit unsigned integer to the right by a given number
 * of positions.
 *
 * @param x The 32-bit unsigned integer to rotate.
 * @param n The number of positions to rotate the bits to the right.
 * @return The result of rotating the bits of x to the right by n positions.
 */
inline uint32_t rotate_right(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

// The mixing function, G, which mixes either a column or a diagonal.
/**
 * Performs the G function of the BLAKE hash algorithm on the given state and
 * message words.
 *
 * @param state The current state of the hash algorithm.
 * @param a The index of the first state word to use in the function.
 * @param b The index of the second state word to use in the function.
 * @param c The index of the third state word to use in the function.
 * @param d The index of the fourth state word to use in the function.
 * @param mx The first message word to use in the function.
 * @param my The second message word to use in the function.
 */
inline void g(
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

inline void round_function(uint32_t state[16], uint32_t m[16]) {
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

/**
 * Permutes the given message block using the BLAKE message permutation.
 *
 * @param m The message block to permute.
 */
inline void permute(uint32_t m[16]) {
    uint32_t permuted[16];
    for (size_t i = 0; i < 16; i++) { permuted[i] = m[MSG_PERMUTATION[i]]; }
    memcpy(m, permuted, sizeof(permuted));
}

/**
 * Compresses a block of data using the BLAKE hash function.
 *
 * @param chaining_value The current chaining value.
 * @param block_words The block of data to compress.
 * @param counter The current counter value.
 * @param BLAKE3_BLOCK_LEN The length of the block in bytes.
 * @param flags The current flag value.
 * @param out The resulting hash value.
 */
inline void compress(const uint32_t chaining_value[8], const uint32_t block_words[16],
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
        IV[0],
        IV[1],
        IV[2],
        IV[3],
        (uint32_t)counter,
        (uint32_t)(counter >> 32),
        BLAKE3_BLOCK_LEN,
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
}

/**
 * Converts an array of little-endian bytes to an array of 32-bit words.
 *
 * @param bytes Pointer to the array of bytes to be converted.
 * @param bytes_len Length of the byte array. Must be a multiple of 4.
 * @param out Pointer to the output array of 32-bit words.
 */
inline void words_from_little_endian_bytes(const void *bytes, size_t bytes_len, uint32_t *out) {
    assert(bytes_len % 4 == 0);
    const uint8_t *u8_ptr = (const uint8_t *)bytes;
    for (size_t i = 0; i < (bytes_len / 4); i++) {
        out[i] = ((uint32_t)(*u8_ptr++));
        out[i] += ((uint32_t)(*u8_ptr++)) << 8;
        out[i] += ((uint32_t)(*u8_ptr++)) << 16;
        out[i] += ((uint32_t)(*u8_ptr++)) << 24;
    }
}

int main(void) {
    char  filename[] = "test_input";
    FILE *input      = fopen(filename, "rb");

    if (input == NULL) {
        printf("Error opening file\n");
        exit(1);
    }

    fseek(input, 0L, SEEK_END);
    size_t num_chunks = ftell(input) >> CHUNK_SIZE_LOG;  // / 1024
    fclose(input);

    int num_threads = 8;
    // TODO prova ad approssimare per eccesso
    // TODO FIX logaritmo
    int level          = (int)log2(num_threads);
    int max_depth      = (int)ceil(log2(num_chunks));
    int sub_tree_depth = max_depth - level;
    int num_leaves     = (int)sub_tree_depth << 1;  // 2^sub_tree_depth

    for (size_t t = 0; t < num_threads; t++) {
        int stride = t * num_leaves << CHUNK_SIZE_LOG;

        FILE *thread_input = fopen(filename, "rb");
        fseek(thread_input, stride, SEEK_SET);

        // reading 4 chunks of 1024 bytes each at a time
        char  *buffer = malloc(4 * CHUNK_SIZE);
        size_t len    = fread(buffer, 1, 4 * CHUNK_SIZE, thread_input);

        if (len == 0) break;  // EOF

        int num_chunks = (int)ceil((double)len / CHUNK_SIZE);  // 1 <= num_chunks <= 4
        if (num_chunks > 4) exit(1);

        for (size_t chunk = 0; chunk < num_chunks; chunk++) {

            // Each chunk of up to 1024 bytes is split into blocks of up to 64 bytes.
            int  num_blocks = 64;
            bool last_chunk = false;
            int  remainder  = (len - CHUNK_SIZE * (num_chunks - 1));  // 0 <= remainder <= 1024
            if (chunk == num_chunks - 1) {
                num_blocks = (int)ceil((double)remainder / BLAKE3_BLOCK_LEN);
                last_chunk = true;
            }

            uint32_t chaining_value[16];

            for (size_t block = 0; block < num_blocks; block++) {
                // The block length b is the number of input bytes in each block,
                // i.e., 64 for all blocks except the last block of the last chunk,
                // which may be short.
                int      block_len = BLAKE3_BLOCK_LEN;
                uint32_t flags     = 0;

                // if we're in the last block of the last chunk
                if (block == num_blocks - 1 && last_chunk) {

                    // if (len % BLAKE3_BLOCK_LEN != 0)
                    // if remainder is not a multiple of 64, last block is not full
                    if (remainder & (BLAKE3_BLOCK_LEN - 1) == 0) {

                        // calculate how many bytes are present
                        block_len = remainder % BLAKE3_BLOCK_LEN;

                        // if last block has less than 64 bytes
                        if (block_len < BLAKE3_BLOCK_LEN) {

                            // pad with zeros
                            memset(&buffer[block * BLAKE3_BLOCK_LEN + block_len], 0,
                                BLAKE3_BLOCK_LEN - block_len);
                        }
                    }
                }
                int access_index = chunk * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;

                // Each block is parsed in little-endian order into message words m0... m15 and
                // compressed
                uint32_t output_blocks[16];
                words_from_little_endian_bytes(
                    &buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

                if (block == 0) flags |= CHUNK_START;
                // The first block of each chunk sets the CHUNK_START flag

                // the last block of each chunk sets the CHUNK_END flag.
                if (block == num_blocks - 1) flags |= CHUNK_END;

                // The counter t for each block is the chunk index i.e., 0 for all blocks in the
                // first chunk, 1 for all blocks in the second chunk, and so on.
                int counter_t = chunk;

                // If a chunk contains only one block, that block sets both CHUNK_START and
                // CHUNK_END.
                if (num_blocks == 1) assert(flags == (CHUNK_START | CHUNK_END));

                // If a chunk is the root of its tree, the last block of that chunk also
                // TODO: sets the ROOT flag.

                // The input chaining value h0 ... h7 for the first block of each chunk is composed
                // of the key words k0 ... k7, or in the base case it's the IV vector.
                // The input chaining value for subsequent blocks is the output of the compression
                // function for the previous block
                const uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);

                // The output of the truncated compression function for the last block in a chunk is
                // the chaining value of that chunk.
                uint32_t out16[16];
                compress(input_chaining_value, output_blocks, counter_t, block_len, flags, out16);
                // copy back the chaining value
                memcpy(chaining_value, out16, sizeof(out16));
            }
        }
        fclose(thread_input);

        return 0;
    }
