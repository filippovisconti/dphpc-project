#include "blake_d.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
// #include <sys/_types/_size_t.h>
#include <sys/types.h>

void blake3_chunk_state_hasher(chunk_state* input_chunks, size_t nchunks, bool has_key,
    const uint8_t key[BLAKE3_KEY_LEN], const char* key_context, uint32_t init_flags,
    uint8_t* output, size_t out_len);
void blake3_chunk_state_slow_hasher(chunk_state* input_chunks, size_t nchunks, bool has_key,
    const uint8_t key[BLAKE3_KEY_LEN], const char* key_context, uint32_t init_flags,
    uint8_t* output, size_t out_len);

void blake(char* test_file, bool has_key, const uint8_t key[BLAKE3_KEY_LEN],
    const char* key_context, uint8_t* output, size_t out_len, Mode mode) {
    // open input stream
    FILE* input_stream = fopen(test_file, "rb");
    if (input_stream == NULL) {
        printf("Can't open file\n");
        exit(1);
    }

    // read input into buf, store_chunks in chunk state
    chunk_state*  input_chunks = (chunk_state*)malloc(MAX_CHUNKS * sizeof(chunk_state));
    size_t        nchunks      = 0;
    unsigned char buf[65536];
    while (1) {
        ssize_t n = fread(buf, 1, sizeof(buf), input_stream);
        if (n > 0) {
            nchunks += store_chunks(input_chunks, nchunks, buf, n);
        } else if (n == 0) {
            break;  // end of file
        } else {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            exit(1);
        }
    }
    fclose(input_stream);

    // call blake3_chunk_state_hasher on chunk_state
    switch (mode) {
        case SINGLE_THREAD:
            blake3_chunk_state_slow_hasher(
                input_chunks, nchunks, has_key, key, key_context, 0, output, out_len);
            break;
        case MULTI_THREAD:
            blake3_chunk_state_hasher(
                input_chunks, nchunks, has_key, key, key_context, 0, output, out_len);
    }
}

void blake3_chunk_state_hasher(chunk_state* input_chunks, size_t nchunks, bool has_key,
    const uint8_t key[BLAKE3_KEY_LEN], const char* key_context, uint32_t init_flags,
    uint8_t* output, size_t out_len) {
    uint32_t master_chaining_value[8];
    uint32_t master_flags = init_flags;
    // // there are 3 options to init: running with key, deriving key, no key
    if (has_key) {
        uint32_t key_words[8];
        words_from_little_endian_bytes(key, BLAKE3_KEY_LEN, key_words);
        master_flags = KEYED_HASH;
        for (int i = 0; i < 8; i++) master_chaining_value[i] = key_words[i];
    } else if (key_context != NULL) {
        master_flags = DERIVE_KEY_CONTEXT;
        chunk_state* key_context_chunks =
            malloc(sizeof(chunk_state) * strlen(key_context) / BLAKE3_CHUNK_LEN);
        size_t num_context_chunks =
            store_chunks(key_context_chunks, 0, key_context, strlen(key_context));

        // compress the context string
        uint8_t context_out[BLAKE3_KEY_LEN];
        blake3_chunk_state_hasher(key_context_chunks, num_context_chunks, false, NULL, NULL,
            master_flags, context_out, BLAKE3_KEY_LEN);
        // set chaining value to the first 8 bytes of that output^
        uint32_t context_key_words[8];
        words_from_little_endian_bytes(context_out, BLAKE3_KEY_LEN, context_key_words);
        for (int i = 0; i < 8; i++) master_chaining_value[i] = context_key_words[i];
        master_flags = DERIVE_KEY_MATERIAL;
    } else {
        for (int i = 0; i < 8; i++) master_chaining_value[i] = IV[i];
    }

    uint32_t compression_outputs[nchunks][CACHE_LINE_SIZE / sizeof(uint32_t)];
    int      blocks_to_compress = BLAKE3_BLOCK_CAP;
    // if input is a single chunk, the last block should not be compressed
    if (nchunks == 1) blocks_to_compress -= 1;
#pragma omp parallel for
    for (int i = 0; i < nchunks; i++) {
        chunk_state* self = (input_chunks + i);

        uint32_t chaining_value[16];
        for (int b = 0; b < blocks_to_compress; b++) {
            uint32_t block_words[16];
            words_from_little_endian_bytes(
                self->block + (b * BLAKE3_BLOCK_LEN), BLAKE3_BLOCK_LEN, block_words);

            uint32_t flags = master_flags;
            if (b == 0) {
                for (size_t i = 0; i < 8; i++) chaining_value[i] = *(master_chaining_value + i);
                flags |= CHUNK_START;
            }
            if (b == BLAKE3_BLOCK_CAP - 1) flags |= CHUNK_END;

            uint32_t out16[16];
            compress(chaining_value, block_words, i, BLAKE3_BLOCK_LEN, flags, out16);
            for (size_t i = 0; i < 16; i++) chaining_value[i] = out16[i];
        }

        // TODO: vectorization speed up possible here
        for (int j = 0; j < 16; j++) compression_outputs[i][j] = *(chaining_value + j);
    }

    if (nchunks == 1) {
        for (int j = 0; j < 8; j++) master_chaining_value[j] = compression_outputs[0][j];
        words_from_little_endian_bytes(input_chunks->block + (15 * BLAKE3_BLOCK_LEN),
            BLAKE3_BLOCK_LEN, compression_outputs[0]);
    }

    free(input_chunks);  // no longer needed after compression

    // creating parent nodes until root is reached
    int      on_compression_stack = nchunks;
    uint32_t next_compression_outputs[on_compression_stack][CACHE_LINE_SIZE / sizeof(uint32_t)];
    while (on_compression_stack > 2) {
        int next_on_compression_stack = 0;
        if (on_compression_stack % 2) {
            memcpy(next_compression_outputs[on_compression_stack / 2],
                compression_outputs[on_compression_stack - 1], sizeof(uint32_t) * 16);
            on_compression_stack--;
            next_on_compression_stack++;
        }

        assert(on_compression_stack % 2 == 0);
#pragma omp parallel for reduction(+ : next_on_compression_stack)
        for (int i = 0; i < on_compression_stack; i += 2) {
            // create parents
            uint32_t parent_words[16];
            // left child
            memcpy(parent_words, compression_outputs[i], sizeof(uint32_t) * 8);
            // right child
            memcpy(parent_words + 8, compression_outputs[i + 1], sizeof(uint32_t) * 8);

            uint32_t out16[16];
            compress(master_chaining_value, parent_words, 0, BLAKE3_BLOCK_LEN,
                master_flags | PARENT, out16);
            memcpy(next_compression_outputs[i / 2], out16, sizeof(uint32_t) * 16);
            next_on_compression_stack++;
        }

        memcpy(compression_outputs, next_compression_outputs,
            on_compression_stack * 16 * sizeof(uint32_t));
        on_compression_stack = next_on_compression_stack;
    }

    assert(on_compression_stack <= 2);
    uint32_t root_words[16];
    uint32_t flags;
    if (on_compression_stack == 1) {
        memcpy(root_words, compression_outputs[0], sizeof(uint32_t) * 16);
        flags = master_flags | CHUNK_END;
    } else {
        memcpy(root_words, compression_outputs[0], sizeof(uint32_t) * 8);
        memcpy(root_words + 8, compression_outputs[1], sizeof(uint32_t) * 8);
        flags = master_flags | PARENT;
    }

    uint64_t output_block_counter = 0;
    // size_t out_len = BLAKE3_OUT_LEN;
    void*    out    = output;
    uint8_t* out_u8 = (uint8_t*)out;

    while (out_len > 0) {
        uint32_t words[16];
        compress(master_chaining_value, root_words, output_block_counter, BLAKE3_BLOCK_LEN,
            flags | ROOT, words);
        for (size_t word = 0; word < 16; word++) {
            for (int byte = 0; byte < 4; byte++) {
                if (out_len == 0) { break; }

                *out_u8 = (uint8_t)(words[word] >> (8 * byte));
                out_u8++;
                out_len--;
            }
        }
        output_block_counter++;
    }
}

void blake3_chunk_state_slow_hasher(chunk_state* input_chunks, size_t nchunks, bool has_key,
    const uint8_t key[BLAKE3_KEY_LEN], const char* key_context, uint32_t init_flags,
    uint8_t* output, size_t out_len) {
    uint32_t master_chaining_value[8];
    uint32_t master_flags = init_flags;
    // // there are 3 options to init: running with key, deriving key, no key
    if (has_key) {
        uint32_t key_words[8];
        words_from_little_endian_bytes(key, BLAKE3_KEY_LEN, key_words);
        master_flags = KEYED_HASH;
        for (int i = 0; i < 8; i++) master_chaining_value[i] = key_words[i];
    } else if (key_context != NULL) {
        master_flags = DERIVE_KEY_CONTEXT;
        chunk_state* key_context_chunks =
            malloc(sizeof(chunk_state) * strlen(key_context) / BLAKE3_CHUNK_LEN);
        size_t num_context_chunks =
            store_chunks(key_context_chunks, 0, key_context, strlen(key_context));

        // compress the context string
        uint8_t context_out[BLAKE3_KEY_LEN];
        blake3_chunk_state_hasher(key_context_chunks, num_context_chunks, false, NULL, NULL,
            master_flags, context_out, BLAKE3_KEY_LEN);
        // set chaining value to the first 8 bytes of that output^
        uint32_t context_key_words[8];
        words_from_little_endian_bytes(context_out, BLAKE3_KEY_LEN, context_key_words);
        for (int i = 0; i < 8; i++) master_chaining_value[i] = context_key_words[i];
        master_flags = DERIVE_KEY_MATERIAL;
    } else {
        for (int i = 0; i < 8; i++) master_chaining_value[i] = IV[i];
    }

    uint32_t compression_outputs[nchunks][CACHE_LINE_SIZE / sizeof(uint32_t)];
    int      blocks_to_compress = BLAKE3_BLOCK_CAP;
    // if input is a single chunk, the last block should not be compressed
    if (nchunks == 1) blocks_to_compress -= 1;
    for (size_t i = 0; i < nchunks; i++) {
        chunk_state* self = (input_chunks + i);

        uint32_t chaining_value[16];
        for (int b = 0; b < blocks_to_compress; b++) {
            uint32_t block_words[16];
            words_from_little_endian_bytes(
                self->block + (b * BLAKE3_BLOCK_LEN), BLAKE3_BLOCK_LEN, block_words);

            uint32_t flags = master_flags;
            if (b == 0) {
                for (size_t i = 0; i < 8; i++) chaining_value[i] = *(master_chaining_value + i);
                flags |= CHUNK_START;
            }
            if (b == BLAKE3_BLOCK_CAP - 1) flags |= CHUNK_END;

            uint32_t out16[16];
            compress(chaining_value, block_words, i, BLAKE3_BLOCK_LEN, flags, out16);
            for (size_t i = 0; i < 16; i++) chaining_value[i] = out16[i];
        }

        // TODO: vectorization speed up possible here
        for (int j = 0; j < 16; j++) compression_outputs[i][j] = *(chaining_value + j);
    }

    if (nchunks == 1) {
        for (int j = 0; j < 8; j++) master_chaining_value[j] = compression_outputs[0][j];
        words_from_little_endian_bytes(input_chunks->block + (15 * BLAKE3_BLOCK_LEN),
            BLAKE3_BLOCK_LEN, compression_outputs[0]);
    }

    free(input_chunks);  // no longer needed after compression

    // creating parent nodes until root is reached
    int      on_compression_stack = nchunks;
    uint32_t next_compression_outputs[on_compression_stack][CACHE_LINE_SIZE / sizeof(uint32_t)];
    while (on_compression_stack > 2) {
        int next_on_compression_stack = 0;
        if (on_compression_stack % 2) {
            memcpy(next_compression_outputs[on_compression_stack / 2],
                compression_outputs[on_compression_stack - 1], sizeof(uint32_t) * 16);
            on_compression_stack--;
            next_on_compression_stack++;
        }

        assert(on_compression_stack % 2 == 0);
        for (int i = 0; i < on_compression_stack; i += 2) {
            // create parents
            uint32_t parent_words[16];
            // left child
            memcpy(parent_words, compression_outputs[i], sizeof(uint32_t) * 8);
            // right child
            memcpy(parent_words + 8, compression_outputs[i + 1], sizeof(uint32_t) * 8);

            uint32_t out16[16];
            compress(master_chaining_value, parent_words, 0, BLAKE3_BLOCK_LEN,
                master_flags | PARENT, out16);
            memcpy(next_compression_outputs[i / 2], out16, sizeof(uint32_t) * 16);
            next_on_compression_stack++;
        }

        memcpy(compression_outputs, next_compression_outputs,
            on_compression_stack * 16 * sizeof(uint32_t));
        on_compression_stack = next_on_compression_stack;
    }

    assert(on_compression_stack <= 2);
    uint32_t root_words[16];
    uint32_t flags;
    if (on_compression_stack == 1) {
        memcpy(root_words, compression_outputs[0], sizeof(uint32_t) * 16);
        flags = master_flags | CHUNK_END;
    } else {
        memcpy(root_words, compression_outputs[0], sizeof(uint32_t) * 8);
        memcpy(root_words + 8, compression_outputs[1], sizeof(uint32_t) * 8);
        flags = master_flags | PARENT;
    }

    uint64_t output_block_counter = 0;
    // size_t out_len = BLAKE3_OUT_LEN;
    void*    out    = output;
    uint8_t* out_u8 = (uint8_t*)out;

    while (out_len > 0) {
        uint32_t words[16];
        compress(master_chaining_value, root_words, output_block_counter, BLAKE3_BLOCK_LEN,
            flags | ROOT, words);
        for (size_t word = 0; word < 16; word++) {
            for (int byte = 0; byte < 4; byte++) {
                if (out_len == 0) { break; }

                *out_u8 = (uint8_t)(words[word] >> (8 * byte));
                out_u8++;
                out_len--;
            }
        }
        output_block_counter++;
    }
}

#if defined MYBLAKE_MAIN
int         main(void) {

    char    filename[] = "input_data/input_64MB.txt";
    size_t  output_len = 128;
    uint8_t output[output_len];

      blake(filename, false, NULL, NULL, output, output_len,
#ifdef USE_OPENMP
        MULTI_THREAD);
#else
        SINGLE_THREAD);
#endif
    printf("\n\nOutput: ");
    for (size_t i = 0; i < output_len; i++) {
        if (i % 32 == 0) printf("\n");
        printf("%02x", output[i]);
    }
    printf("\n");
    return 0;
}
#endif
