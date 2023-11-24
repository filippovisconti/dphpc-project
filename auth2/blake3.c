#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

// #include "include/reference_impl.h"

#define CHUNK_START         1 << 0
#define CHUNK_END           1 << 1
#define PARENT              1 << 2
#define ROOT                1 << 3
#define MAX_CHUNKS          1024
#define BLAKE3_CHUNK_LEN    1024
#define BLAKE3_BLOCK_LEN    64
#define BLAKE3_BLOCK_CAP    16
#define BLAKE3_OUT_LEN      32

typedef struct chunk_state {
    char block[BLAKE3_CHUNK_LEN];
    uint8_t block_len;
} chunk_state;

static uint32_t IV[8] = {
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
};

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

int store_chunks(chunk_state* chunks, size_t pos, 
                 const void * input, size_t input_len) {
    int counter = 0;
    int bytes_copied = 0;
    while (bytes_copied < input_len) {
        chunk_state* curr_chunk = chunks + pos;
        // todo: only works for even blocks
        memcpy(curr_chunk->block, input + bytes_copied, BLAKE3_CHUNK_LEN);
        counter++;
        pos++;
        bytes_copied += BLAKE3_CHUNK_LEN;
    }

    return counter;
}

inline static uint32_t rotate_right(uint32_t x, int n) {
  return (x >> n) | (x << (32 - n));
}

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

static size_t MSG_PERMUTATION[16] = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

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
      IV[0],
      IV[1],
      IV[2],
      IV[3],
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
}

int main(int argc, char *argv[]) {
    // uint8_t key[BLAKE3_KEY_LEN];
    // bool has_key = false;
    // const char *derive_key_context = NULL;
    if (argc <= 1) return 1;
    char* test_file = argv[1];

    // char filename[] = test_file;
    FILE *input_stream = fopen(test_file, "rb");
    if (input_stream == NULL) {
        printf("Can't open file\n");
        exit(1);
    }

    // // there are 3 options to init: running with key, deriving key, no key
    // if (has_key) {
    //     blake3_hasher_init_keyed(&hasher, key);
    // } else if (derive_key_context != NULL) {
    //     blake3_hasher_init_derive_key(&hasher, derive_key_context);
    // } else {
    //     blake3_hasher_init(&hasher);
    // }

    // read in file into buffer
    unsigned char buf[65536];

    chunk_state* input_chunks = (chunk_state*) malloc(MAX_CHUNKS * sizeof(chunk_state));
    size_t nchunks = 0;

    // read in file and store chunks in array
    while (1) {
        ssize_t n = fread(buf, 1, sizeof(buf), input_stream);
        // printf("n: %i\n", n);
        if (n > 0) {
            // printf("Buf: %s\n", buf);
            nchunks += store_chunks(input_chunks, nchunks, buf, n);
        } else if (n == 0) {
            break; // end of file
        } else {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            exit(1);
        }
    }
    // TODO: set chunk_start and chunk_end flags?

    // TESTING
    // for (size_t i = 0; i < nchunks; i++) {
    //     char Chunk[BLAKE3_CHUNK_LEN + 1];
    //     Chunk[BLAKE3_CHUNK_LEN] = '\0';
    //     memcpy(Chunk, (input_chunks+i)->block, BLAKE3_CHUNK_LEN);

    //     printf("Chunk %i: %s\n", i, (input_chunks+i)->block);
    // }

    clock_t begin = clock();

    // compressing chunks, storing the chaining values
    // ATTN: should be parallel region
    uint32_t compression_outputs[nchunks][16];
    for (int i = 0; i < nchunks; i++) {
        chunk_state* self = (input_chunks + i);

        uint32_t chaining_value[8];
        for (int b = 0; b < BLAKE3_BLOCK_CAP; b++) {
            uint32_t block_words[16];
            words_from_little_endian_bytes(self->block + (b*BLAKE3_BLOCK_LEN), BLAKE3_BLOCK_LEN, block_words);
            
            uint32_t flags = 0;
            if (b == 0) {
                memcpy(chaining_value, IV, 8 * sizeof(uint32_t));
                flags = CHUNK_START;
            }
            if (b == BLAKE3_BLOCK_CAP - 1) flags = CHUNK_END;
            
            uint32_t out16[16];
            compress(chaining_value, block_words, i, BLAKE3_BLOCK_LEN, flags, out16);
            // printf("Chunk %i, Block %i compression, counter = %i\n, flags = %i", i, b, i, flags);
            // printf("    block words: ");
            // for (size_t i = 0; i < 16; i++) {
            //     printf("%02x", block_words[i]);
            // }
            // printf("\n    chaining value: ");
            // for (size_t i = 0; i < 8; i++) {
            //     printf("%02x", chaining_value[i]);
            // }
            // printf("\n    out: ");
            // for (size_t i = 0; i < 16; i++) {
            //     printf("%02x", out16[i]);
            // }
            // printf("\n");

            for (int i = 0; i < 8; i++) chaining_value[i] = out16[i];
        }
        memcpy(compression_outputs[i], chaining_value, sizeof(uint32_t) * 16);
    }

    // TESTING
    // printf("Compression Outputs: \n");
    // for (int n = 0; n < nchunks; n++) {
    //     printf("    chunk %i:", n);
    //     for (size_t i = 0; i < 16; i++) {
    //         printf("%02x", compression_outputs[n][i]);
    //     }
    //     printf("\n");
    // }

    // creating parent nodes until root is reached
    int on_compression_stack = nchunks;
    uint32_t next_compression_outputs[on_compression_stack][16];
    while (on_compression_stack > 2) {
        // printf("Compression Stack: %i elements\n", on_compression_stack);
        // for (int n = 0; n < on_compression_stack; n++) {
        //     printf("    element %i:", n);
        //     for (size_t i = 0; i < 16; i++) {
        //         printf("%02x", compression_outputs[n][i]);
        //     }
        //     printf("\n");
        // }
        int next_on_compression_stack = 0;
        if (on_compression_stack % 2) {
            // TODO: the last chunk will not merge to form a parent, just add it to the next outputs
            memcpy(next_compression_outputs[nchunks/2], compression_outputs[nchunks - 1], sizeof(uint32_t) * 16);
            on_compression_stack--;
            next_on_compression_stack++;
        }

        // ATTN: should be parallel region
        assert(on_compression_stack % 2 == 0);
        for (int i = 0; i < on_compression_stack; i+=2) {
            // create parents
            uint32_t parent_words[16];
            // left child
            memcpy(parent_words, compression_outputs[i], sizeof(uint32_t) * 8);
            // right child
            memcpy(parent_words + 8, compression_outputs[i + 1], sizeof(uint32_t) * 8);

            uint64_t counter   = 0;      // Always 0 for parent nodes.
            uint32_t flags = PARENT;

            uint32_t out16[16];
            // TODO: currently the chaining value (ie key words) are just the default hashing mode
            compress(IV, parent_words, counter, BLAKE3_BLOCK_LEN, flags, out16);
            memcpy(next_compression_outputs[i/2], out16, sizeof(uint32_t) * 16);
            next_on_compression_stack++;
        }
        // ATTN: end parallel region

        memcpy(compression_outputs, next_compression_outputs, on_compression_stack * 16 * sizeof(uint32_t));
        on_compression_stack = next_on_compression_stack;
    }

    // reached root
    // compression_outputs[0] is the root
    assert(on_compression_stack <= 2);
    uint32_t root_words[16];
    uint32_t flags;
    if (on_compression_stack == 1) {
        memcpy(root_words, compression_outputs[0], sizeof(uint32_t) * 16);
        flags = CHUNK_END;
    } else {
        memcpy(root_words, compression_outputs[0], sizeof(uint32_t) * 8);
        memcpy(root_words + 8, compression_outputs[1], sizeof(uint32_t) * 8);
        flags = PARENT;
    }

    // printf("found root: ");
    // for (size_t i = 0; i < 16; i++) {
    //     printf("%02x", root_words[i]);
    // }
    // printf("\n");

    uint64_t output_block_counter = 0;
    size_t out_len = BLAKE3_OUT_LEN;
    uint8_t output[out_len];
    void *out = output;
    uint8_t *out_u8 = (uint8_t *)out;

    while (out_len > 0) {
        uint32_t words[16];
        // printf("flags: %i\n", flags | ROOT); // ATTN
        compress(IV, root_words, output_block_counter, BLAKE3_BLOCK_LEN, flags | ROOT, words);
        for (size_t word = 0; word < 16; word++) {
            for (int byte = 0; byte < 4; byte++) {
                if (out_len == 0) { break; }

                *out_u8 = (uint8_t)(words[word] >> (8 * byte));
                out_u8++;
                out_len--;
            }
        }
    }

    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;

    printf("time: %.5f\n", time_spent);
    for (size_t i = 0; i < BLAKE3_OUT_LEN / sizeof(uint8_t); i++) 
        printf("%02x", output[i]);
    printf("\n");

    return 0;
}