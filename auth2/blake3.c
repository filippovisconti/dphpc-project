#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

// #include "include/reference_impl.h"

#define CHUNK_START         1 << 0
#define CHUNK_END           1 << 1
#define PARENT              1 << 2
#define ROOT                1 << 3
#define MAX_CHUNKS          64
#define BLAKE3_CHUNK_LEN    1024
#define BLAKE3_BLOCK_LEN    64
#define BLAKE3_BLOCK_CAP    16
#define BLAKE3_OUT_LEN      32

typedef struct _blake3_chunk_state {
  uint32_t chaining_value[8];
  uint64_t chunk_counter;
  char block[BLAKE3_CHUNK_LEN];
  uint8_t block_len;
  uint8_t blocks_compressed;
  uint32_t flags;
} _blake3_chunk_state;

typedef struct output {
    uint32_t input_chaining_value[8];
    uint32_t block_words[16];
    uint64_t counter;
    uint32_t block_len;
    uint32_t flags;
} output;

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

int store_chunks(_blake3_chunk_state* chunks, size_t pos, 
                 const void * input, size_t input_len) {
    int counter = 0;
    int bytes_copied = 0;
    while (bytes_copied < input_len) {
        printf("    bytes_copied = %i; iteration pos: %i\n", input_len, pos);
        _blake3_chunk_state* curr_chunk = chunks + pos;
        // todo: only works for even blocks
        memcpy(curr_chunk->block, input + bytes_copied, BLAKE3_CHUNK_LEN);
        memcpy(curr_chunk->chaining_value, IV, 8 * sizeof(uint32_t));
        curr_chunk->block_len = BLAKE3_BLOCK_LEN;
        curr_chunk->chunk_counter = pos;
        counter++;
        pos++;
        bytes_copied += BLAKE3_CHUNK_LEN;
    }

    return counter;
}

// inline static void compress(const uint32_t chaining_value[8], const uint32_t block_words[16],
//     uint64_t counter, uint32_t block_len, uint32_t flags, uint32_t out[16]) {
//   uint32_t state[16] = {
//       chaining_value[0],
//       chaining_value[1],
//       chaining_value[2],
//       chaining_value[3],
//       chaining_value[4],
//       chaining_value[5],
//       chaining_value[6],
//       chaining_value[7],
//       IV[0],
//       IV[1],
//       IV[2],
//       IV[3],
//       (uint32_t)counter,
//       (uint32_t)(counter >> 32),
//       block_len,
//       flags,
//   };
//   uint32_t block[16];
//   memcpy(block, block_words, sizeof(block));

//   round_function(state, block);  // round 1
//   permute(block);
//   round_function(state, block);  // round 2
//   permute(block);
//   round_function(state, block);  // round 3
//   permute(block);
//   round_function(state, block);  // round 4
//   permute(block);
//   round_function(state, block);  // round 5
//   permute(block);
//   round_function(state, block);  // round 6
//   permute(block);
//   round_function(state, block);  // round 7

//   for (size_t i = 0; i < 8; i++) {
//     state[i] ^= state[i + 8];
//     state[i + 8] ^= chaining_value[i];
//   }

//   memcpy(out, state, sizeof(state));
// }

int main(void) {
    // uint8_t key[BLAKE3_KEY_LEN];
    // bool has_key = false;
    // const char *derive_key_context = NULL;
    
    char  filename[] = "test_input";
    FILE *input_stream = fopen(filename, "rb");
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

    _blake3_chunk_state* input_chunks = (_blake3_chunk_state*) malloc(MAX_CHUNKS * sizeof(_blake3_chunk_state));
    size_t nchunks = 0;

    // read in file and store chunks in array
    while (1) {
        ssize_t n = fread(buf, 1, sizeof(buf), input_stream);
        printf("n: %i\n", n);
        if (n > 0) {
            printf("Buf: %s\n", buf);
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

    // compressing chunks, storing the chaining values
    // ATTN: should be parallel region
    uint32_t compression_outputs[nchunks][16];
    for (int i = 0; i < nchunks; i++) {
        _blake3_chunk_state* self = (input_chunks + i);

        const uint32_t chaining_value[8];
        for (int b = 0; b < BLAKE3_BLOCK_CAP; b++) {
            uint32_t block_words[16];
            words_from_little_endian_bytes(self->block[b*BLAKE3_BLOCK_LEN], BLAKE3_BLOCK_LEN, block_words);
            
            uint32_t flags;
            if (b == 0) {
                memcpy(chaining_value, IV, 8 * sizeof(uint32_t));
                flags = CHUNK_START;
            }
            if (b == BLAKE3_BLOCK_LEN - 1) flags = CHUNK_END;
            
            uint32_t out16[16];
            compress(chaining_value, block_words, i, BLAKE3_BLOCK_LEN, flags, out16);
            memcpy(chaining_value, out16, sizeof(uint32_t) * 16);
        }
        memcpy(compression_outputs[i], chaining_value, sizeof(uint32_t) * 16);
    }

    // creating parent nodes until root is reached
    int on_compression_stack = nchunks;
    uint32_t next_compression_outputs[on_compression_stack][16];
    while (on_compression_stack > 1) {
        int next_on_compression_stack = 0;
        if (on_compression_stack % 2) {
            // TODO: the last chunk will not merge to form a parent, just add it to the next outputs
            memcpy(next_compression_outputs[nchunks/2], compression_outputs[nchunks - 1], sizeof(uint32_t) * 16);
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
            memcpy(parent_words[8], compression_outputs[i + 1], sizeof(uint32_t) * 8);

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
    assert(on_compression_stack == 1); // single node which is the root
    uint64_t output_block_counter = 0;
    size_t out_len = BLAKE3_OUT_LEN;
    uint8_t output[out_len];
    uint8_t *out_u8 = output;

    while (out_len > 0) {
        uint32_t words[16];
        compress(IV, compression_outputs[0], output_block_counter, BLAKE3_BLOCK_LEN, ROOT, words);
        for (size_t word = 0; word < 16; word++) {
            for (int byte = 0; byte < 4; byte++) {
                if (out_len == 0) { return; }

                *out_u8 = (uint8_t)(words[word] >> (8 * byte));
                out_u8++;
                out_len--;
            }
        }
    }

    printf("Output: ");
    for (size_t i = 0; i < out_len; i++) 
        printf("%02x", output[i]);
    printf("\n");

    return 0;
}