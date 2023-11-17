#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

// #include "include/reference_impl.h"

#define CHUNK_START         1 << 0
#define CHUNK_END           1 << 1
#define MAX_CHUNKS          64
#define BLAKE3_CHUNK_LEN    1024
#define BLAKE3_BLOCK_LEN    64

typedef struct _blake3_chunk_state {
  uint32_t chaining_value[8];
  uint64_t chunk_counter;
  char block[BLAKE3_CHUNK_LEN];
  uint8_t block_len;
  uint8_t blocks_compressed;
  uint32_t flags;
} _blake3_chunk_state;

int store_chunks(_blake3_chunk_state* chunks, size_t pos, 
                 const void * input, size_t input_len) {
    int counter = 0;
    int bytes_copied = 0;
    while (bytes_copied < input_len) {
        printf("    bytes_copied = %i; iteration pos: %i\n", input_len, pos);
        _blake3_chunk_state* curr_chunk = chunks + pos;
        // todo: only works for even blocks
        memcpy(curr_chunk->block, input + bytes_copied, BLAKE3_CHUNK_LEN);
        curr_chunk->block_len = BLAKE3_BLOCK_LEN;
        curr_chunk->chunk_counter = pos;
        counter++;
        pos++;
        bytes_copied += BLAKE3_CHUNK_LEN;
    }

    return counter;
}

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

    // // initialize hasher
    // blake3_hasher hasher;

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
    /*
    TODO: to imlement parallelization, best to know the message length instead of running a while
    loop on the stream. Then, further parallelization can occur in some for loop.
    Does hasher_update need to have the results of previous blocks?
    
    */

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

    //TESTING
    for (size_t i = 0; i < nchunks; i++) {
        char Chunk[BLAKE3_CHUNK_LEN + 1];
        Chunk[BLAKE3_CHUNK_LEN] = '\0';
        memcpy(Chunk, (input_chunks+i)->block, BLAKE3_CHUNK_LEN);

        printf("Chunk %i: %s\n", i, (input_chunks+i)->block);
    }

    // all input chunks are initialized, use openmp to compress
    // each chunk in parallel

    // tree depth

    // how to store the output of each chunk / know how to create parents?

    // Finalize the hash.
    // uint8_t output[BLAKE3_OUT_LEN];
    // blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);

    // Print the hash as hexadecimal.
    // for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
    //     printf("%02x", output[i]);
    // }
    // printf("\n");

    return 0;
}