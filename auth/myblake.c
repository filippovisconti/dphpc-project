
#include "myblake.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) < (y) ? (y) : (x))

#ifndef BENCHMARK
#define myprintf printf
#else
#define myprintf(...) (void)0
#endif

uint32_t base_IV[8] = {
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
};

uint32_t *IV = base_IV;

size_t MSG_PERMUTATION[16] = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

static inline uint32_t rotate_right(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static inline void g(
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

static inline void round_function(uint32_t state[16], uint32_t m[16]) {
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

static inline void permute(uint32_t m[16]) {
    uint32_t permuted[16];
    for (size_t i = 0; i < 16; i++) { permuted[i] = m[MSG_PERMUTATION[i]]; }
    memcpy(m, permuted, sizeof(permuted));
}
int                tmp2 = 0;
static inline void compress(const uint32_t chaining_value[8], const uint32_t block_words[16],
    uint64_t counter, uint32_t block_len, uint32_t flags, uint32_t out[16]) {
    (void)block_len;
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

    printf("%2d Ch val: ", tmp2++);
    for (size_t i = 0; i < 8; i++) printf("%08x ", chaining_value[i]);
    printf("\n");
    printf("Block:\n");
    for (size_t i = 0; i < 8; i++) printf("%08x ", block_words[i]);
    printf("\n");
    for (size_t i = 8; i < 16; i++) printf("%08x ", block_words[i]);
    printf("\n");
    printf("flags: %08x\n", flags);

    for (size_t i = 0; i < 8; i++) {
        state[i] ^= state[i + 8];
        state[i + 8] ^= chaining_value[i];
    }

    memcpy(out, state, sizeof(state));
}

static inline void words_from_little_endian_bytes(
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

static inline int next_multiple_of_64(int n){
    return (n + 63) & ~63;
}

static inline int get_num_chunks(char *filename) {
    myprintf("using %s\n", filename);
    FILE *input = fopen(filename, "rb");
    assert(input != NULL);
    fseek(input, 0L, SEEK_END);
    int size = ftell(input);
    fclose(input);

    myprintf("input size: %d\n", size);

    int new_size = next_multiple_of_64(size);
    if (size != new_size) {
        myprintf("new size: %d\n", new_size);
        int res = truncate(filename, new_size);  // truncate output file to power of 2 size
        assert(res == 0);
        myprintf("truncated to %dB\n", new_size);
    } else myprintf("size is already a power of 2\n");
    int  num_chunks        = new_size >> CHUNK_SIZE_LOG;  // divide by 1024
    bool chunks_pow_of_two = (num_chunks & (num_chunks - 1)) == 0;

    assert(chunks_pow_of_two);
    return MAX(num_chunks, 1);
}

static inline void write_output(uint32_t message_words[16], uint32_t counter_t, uint32_t flags,
    uint8_t *output, size_t output_len) {
    uint8_t *running_output     = output;
    size_t   running_output_len = output_len;
    assert(output != NULL);
    memset(output, 0, output_len);

    bool stop = false;
    while (running_output_len > 0) {
        uint32_t words[16];
        compress(IV, message_words, ++counter_t, 64, flags, words);

        stop = false;
        for (size_t word = 0; word < 16 && !stop; word++) {
            for (int byte = 0; byte < 4; byte++) {
                if (output_len == 0 || running_output_len == 0) {
                    myprintf("FINISHED\n");
                    stop = true;
                    break;
                };

                *running_output = (uint8_t)(words[word] >> (8 * byte));
                running_output++;
                running_output_len--;
            }
        }
    }
}

static inline int set_num_threads(int num_chunks) {
#ifdef USE_OPENMP
    int num_threads = omp_get_max_threads();
    assert(num_threads > 1);
    myprintf("[OMP] Number of available threads: %d\n", num_threads);

    num_threads = 1 << (int)log2(num_threads);  // get largest power of 2 smaller than num_threads
    omp_set_dynamic(0);

    while (num_threads > (num_chunks >> 3)) {
        num_threads >>= 1;
        if (num_threads == 2) break;
        myprintf("%d %d %d\n", num_threads, num_chunks, num_chunks >> 2);
    }

    omp_set_num_threads(num_threads);

    // TODO: not worth parallelizing, use only one thread

    myprintf("[OMP] Number of usable threads: %d\n", omp_get_max_threads());
    assert(omp_get_max_threads() == num_threads);
    myprintf("[OMP] Running with num_threads: %d\n", num_threads);
#else
    (void)num_chunks;
    int num_threads = 2;
#endif
    return num_threads;
}

static inline void compute_chunk_chaining_values(char *read_buffer, int num_blocks,
    uint64_t counter_t, int chunk, /*int remaining, bool last_chunk,*/ uint32_t chaining_value[8],
    uint32_t base_flags) {

    for (int block = 0; block < num_blocks; block++) {
        // pad_block_if_necessary(block, num_blocks, remaining, last_chunk, read_buffer);

        uint32_t output_blocks[16];
        int      access_index = (chunk % 4) * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
        words_from_little_endian_bytes(&read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

        uint32_t flags = base_flags;
        if (block == 0) flags |= CHUNK_START;
        if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;

        if (num_blocks == 1) assert(flags == (CHUNK_START | CHUNK_END));

        const uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);
        uint32_t        out16[16];
        compress(input_chaining_value, output_blocks, counter_t, BLAKE3_BLOCK_LEN, flags, out16);

        memcpy(chaining_value, out16, sizeof(out16) >> 1);
    }
}

static inline void short_inputs(
    char *filename, uint8_t *output, size_t output_len, int num_chunks, uint32_t base_flags) {

    FILE *input = fopen(filename, "rb");
    assert(input != NULL);
    uint32_t(*chunk_chaining_values)[8] = calloc(num_chunks, sizeof(uint32_t[8]));
    assert(chunk_chaining_values != NULL);

    int chunk         = 0;
    int chunk_counter = 0;
    int read_size     = num_chunks << CHUNK_SIZE_LOG;

    uint32_t output_blocks[16];
    uint32_t save_chaining_value[8];  // store output here
    memcpy(save_chaining_value, base_IV, sizeof(save_chaining_value));
    uint32_t save_flags = base_flags;
    while (!feof(input) && read_size > 0) {
        char  *read_buffer = calloc(8, CHUNK_SIZE);
        size_t len         = fread(read_buffer, 1, 8 * CHUNK_SIZE, input);
        if (len == 0) break;  // EOF
        len = strlen(read_buffer);

        read_size -= len;

        int num_read_chunks = CEIL_DIV(len, CHUNK_SIZE);
        assert(num_read_chunks <= 8);

        // START BASE CHUNK PROCESSING
        for (int i = 0; i < num_read_chunks; i++, chunk++) {
            int num_blocks = 16;
            int remaining  = (len - CHUNK_SIZE * (num_read_chunks - 1));
            myprintf("remaining: %d\n", remaining);
            // 0 <= remainder <= 1024
            if (chunk == (num_read_chunks - 1)) {
                num_blocks = CEIL_DIV(remaining, BLAKE3_BLOCK_LEN);
                myprintf("num_blocks: %d\n", num_blocks);
            }

            uint64_t counter_t = chunk;
            uint32_t chaining_value[8];  // store output here
                                         // if (num_chunks == 1) base_flags |= ROOT | PARENT;
            for (int block = 0; block < num_blocks; block++) {
                int access_index = chunk * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
                words_from_little_endian_bytes(
                    &read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

                uint32_t flags = base_flags;
                if (block == 0) flags |= CHUNK_START;
                if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;

                if (num_blocks == 1)
                    assert((flags & (CHUNK_START | CHUNK_END)) == (CHUNK_START | CHUNK_END));
                if (num_chunks == 1 && (block == num_blocks - 1)) { flags |= ROOT; }
                if (num_chunks == 1) {
                    save_flags = flags;
                    if (num_blocks != 1)
                        memcpy(save_chaining_value, chaining_value, sizeof(chaining_value));
                }
                const uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);
                uint32_t        out16[16];
                compress(
                    input_chaining_value, output_blocks, counter_t, BLAKE3_BLOCK_LEN, flags, out16);

                memcpy(chaining_value, out16, sizeof(out16) >> 1);

                remaining -= BLAKE3_BLOCK_LEN;
            }
            assert(chunk_counter <= ((int)num_chunks - 1));
            for (int j = 0; j < 8; j++) chunk_chaining_values[chunk_counter][j] = chaining_value[j];
            chunk_counter++;
            printf("chunk_counter: %d\n", chunk_counter);
        }
        free(read_buffer);
    }
    if (num_chunks == 1) {
        myprintf("num_chunks == 1\n");
        base_flags |= ROOT;
        int counter_t = -1;

        myprintf("output_len: %ld\n", output_len);
        uint8_t *running_output     = output;
        size_t   running_output_len = output_len;
        assert(output != NULL);
        memset(output, 0, output_len);

        bool stop = false;
        while (running_output_len > 0) {
            uint32_t words[16];
            compress(save_chaining_value, output_blocks, ++counter_t, BLAKE3_BLOCK_LEN, save_flags,
                words);

            stop = false;
            for (size_t word = 0; word < 16 && !stop; word++) {
                for (int byte = 0; byte < 4; byte++) {
                    if (output_len == 0 || running_output_len == 0) {
                        myprintf("FINISHED\n");
                        stop = true;
                        break;
                    };

                    *running_output = (uint8_t)(words[word] >> (8 * byte));
                    running_output++;
                    running_output_len--;
                }
            }
        }

        free(chunk_chaining_values);
        return;
    }
    // PARENT PROCESSING
    int      current_number_of_nodes = num_chunks;
    uint32_t message_words[16];  // first 8 is left child, second 8 is the right one;

    uint32_t(*buffer_a)[8] = (uint32_t(*)[8])calloc(num_chunks / 2, sizeof(uint32_t[8]));
    uint32_t(*buffer_b)[8] = (uint32_t(*)[8])calloc(num_chunks / 4, sizeof(uint32_t[8]));
    assert(buffer_a != NULL);
    assert(buffer_b != NULL);

    uint32_t *out16 = malloc(16 * sizeof(uint32_t));
    assert(out16 != NULL);
    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
        memcpy(message_words + 0, (&chunk_chaining_values[2 * i + 0]), 32);
        memcpy(message_words + 8, (&chunk_chaining_values[2 * i + 1]), 32);

        uint32_t out16[16];
        base_flags |= PARENT;
        if (current_number_of_nodes <= 2) base_flags |= ROOT;
        compress(IV, message_words, 0, 64, base_flags, out16);

        for (int j = 0; j < 8; j++) buffer_a[i][j] = out16[j];
    }
    free(out16);

    current_number_of_nodes >>= 1;
    free(chunk_chaining_values);
    if (current_number_of_nodes == 1) write_output(buffer_a[0], 0, base_flags, output, output_len);
    else {
        uint8_t a_or_b = 1;
        while (current_number_of_nodes > 1) {
            if (current_number_of_nodes <= 2) base_flags |= ROOT;

            for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
                if (a_or_b) {
                    memcpy(message_words + 0, &buffer_a[2 * i][0], 32);
                    memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
                } else {
                    memcpy(message_words + 0, &buffer_b[2 * i][0], 32);
                    memcpy(message_words + 8, &buffer_b[2 * i + 1][0], 32);
                }

                uint32_t out16[16];
                compress(IV, message_words, 0, 64, base_flags, out16);

                memcpy((a_or_b) ? &buffer_b[i] : &buffer_a[i], out16, sizeof(out16) >> 1);
            }
            current_number_of_nodes >>= 1;
            a_or_b = !a_or_b;
        }
    }
    free(buffer_a);
    free(buffer_b);

    assert(current_number_of_nodes == 1);
    int counter_t = -1;
    write_output(message_words, counter_t, base_flags, output, output_len);
}

void myblake(char *filename, uint8_t *output, size_t output_len, bool has_key, uint8_t *key,
    const char *derive_key_context) {

    uint32_t base_flags = 0;
    if (has_key) {
        assert(key != NULL);
        assert(derive_key_context == NULL);

        IV = malloc(8 * sizeof(uint32_t));
        assert(IV != NULL);
        // output capped at 256 bits
        words_from_little_endian_bytes(key, BLAKE3_KEY_LEN, IV);
        base_flags = KEYED_HASH;

    } else if (derive_key_context != NULL) {
        assert(!has_key);

        char fn[] = "tmp.txt";  // temporary file

        FILE *f = fopen(fn, "w");
        fwrite(derive_key_context, BLAKE3_KEY_LEN, 1, f);  // write context to file
        fclose(f);

        uint8_t *context_key_words = malloc(BLAKE3_KEY_LEN);
        myblake(fn, context_key_words, BLAKE3_KEY_LEN, false, NULL, NULL);  // hash context
        IV = malloc(8 * sizeof(uint32_t));
        assert(IV != NULL);
        words_from_little_endian_bytes(context_key_words, BLAKE3_KEY_LEN, IV);
        base_flags = DERIVE_KEY_CONTEXT;
        remove(fn);  // delete temporary file
    }

    int num_chunks = get_num_chunks(filename);

    if (num_chunks < 8) {
        myprintf("running short version, on num_chunks: %d\n", num_chunks);
        short_inputs(filename, output, output_len, num_chunks, base_flags);
        if (has_key || derive_key_context != NULL) free(IV);

        return;
    }

    int num_threads = set_num_threads(num_chunks);

    size_t level     = (int)log2(num_threads);  // tree depth in which we have one node per thread
    size_t max_depth = (int)ceil(log2(num_chunks));  // maximum depth of the tree
    size_t sub_tree_depth = max_depth - level;       // depth of each thread's subtree
    size_t num_leaves     = 1 << sub_tree_depth;     // 2^sub_tree_depth

#ifdef USE_OPENMP
    uint32_t parent_chaining_values[num_threads][8];
#pragma omp parallel
#else
    uint32_t parent_chaining_values[2][8];
    for (int thread_num = 0; thread_num < 2; thread_num++)
#endif
    {
#ifdef USE_OPENMP
        int thread_num = omp_get_thread_num();
        assert(thread_num <= num_threads);
#endif

        int   stride       = (thread_num * num_leaves) << CHUNK_SIZE_LOG;
        FILE *thread_input = fopen(filename, "rb");
        fseek(thread_input, stride, SEEK_SET);

        uint32_t(*chunk_chaining_values)[8] = calloc(num_leaves, sizeof(uint32_t[8]));
        assert(chunk_chaining_values != NULL);

        int   chunk         = 0;
        int   chunk_counter = 0;
        int   read_size     = num_leaves << CHUNK_SIZE_LOG;
        char *read_buffer   = malloc(4 * CHUNK_SIZE);
        while (!feof(thread_input) && read_size > 0) {
            size_t len = fread(read_buffer, 1, 4 * CHUNK_SIZE, thread_input);
            if ((len == 0) || (len > (num_leaves << CHUNK_SIZE_LOG))) break;  // EOF

            read_size -= len;

            int num_read_chunks = CEIL_DIV(len, CHUNK_SIZE);
            assert(num_read_chunks <= 4);

            // START BASE CHUNK PROCESSING
            for (int i = 0; i < num_read_chunks; i++, chunk++) {
                int num_blocks = 16;
                // bool last_chunk = false;
                int remaining = (len - CHUNK_SIZE * (num_read_chunks - 1));
                // 0 <= remainder <= 1024
                if (chunk == (num_read_chunks - 1)) {
                    num_blocks = CEIL_DIV(remaining, BLAKE3_BLOCK_LEN);
                    // last_chunk = true;
                }

                uint64_t counter_t = chunk + num_leaves * thread_num;
                uint32_t chaining_value[8];  // store output here
                compute_chunk_chaining_values(
                    read_buffer, num_blocks, counter_t, chunk, /*remaining,
last_chunk,*/ chaining_value, base_flags);

                // save last block's cv for parent processing
                assert(chunk_counter < (int)num_leaves);
                for (int j = 0; j < 8; j++)
                    chunk_chaining_values[chunk_counter][j] = chaining_value[j];
                chunk_counter++;
            }
            // END BASE CHUNK PROCESSING
        }
        free(read_buffer);
        fclose(thread_input);
        // START PARENT PROCESSING
        const uint32_t *input_chaining_value    = IV;  // input ch val is the keywords
        int             current_number_of_nodes = num_leaves;
        uint32_t        message_words[16];  // first 8 is left child, second 8 is the right one;

        uint32_t(*buffer_a)[8] = (uint32_t(*)[8])calloc(num_leaves / 2, sizeof(uint32_t[8]));
        uint32_t(*buffer_b)[8] = (uint32_t(*)[8])calloc(num_leaves / 4, sizeof(uint32_t[8]));
        assert(buffer_a != NULL);
        assert(buffer_b != NULL);

        uint8_t a_or_b = 1;

        for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
            memcpy(message_words + 0, (&chunk_chaining_values[2 * i + 0]), 32);
            memcpy(message_words + 8, (&chunk_chaining_values[2 * i + 1]), 32);

            uint32_t out16[16];
            compress(input_chaining_value, message_words, 0, 64, PARENT | base_flags, out16);

            memcpy(buffer_a + i, out16, sizeof(out16) >> 1);
        }
        current_number_of_nodes >>= 1;
        free(chunk_chaining_values);

        while (current_number_of_nodes > 1) {
            for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
                if (a_or_b) {
                    memcpy(message_words + 0, &buffer_a[2 * i][0], 32);
                    memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
                } else {
                    memcpy(message_words + 0, &buffer_b[2 * i][0], 32);
                    memcpy(message_words + 8, &buffer_b[2 * i + 1][0], 32);
                }

                uint32_t out16[16];
                compress(input_chaining_value, message_words, 0, 64, PARENT | base_flags, out16);

                memcpy((a_or_b) ? &buffer_b[i] : &buffer_a[i], out16, sizeof(out16) >> 1);
            }
            current_number_of_nodes >>= 1;
            a_or_b = !a_or_b;
        }
#ifdef ENABLE_PRINT
        if (current_number_of_nodes != 1)
            printf("current_number_of_nodes: %d\n", current_number_of_nodes);
#endif
        assert(current_number_of_nodes == 1);
        assert(thread_num < num_threads);

        for (int j = 0; j < 8; j++)
            parent_chaining_values[thread_num][j] = (a_or_b) ? buffer_a[0][j] : buffer_b[0][j];

        free(buffer_a);
        free(buffer_b);
    }
    // printf("END PARALLEL REGION\n");
    int      counter_t = 0;                    // always 0 for parent nodes
    uint32_t flags     = PARENT | base_flags;  // set parent flag

    // const uint32_t */* input_chaining_value */ = IV;  // input ch val is the
    // keywords
    uint32_t message_words[16];  // first 8 is left child, second 8 is the right one;
    int      current_number_of_nodes = num_threads;

    uint32_t(*buffer_a)[8] = calloc(num_leaves / 2, sizeof(uint32_t[8]));
    uint32_t(*buffer_b)[8] = calloc(num_leaves / 4, sizeof(uint32_t[8]));
    uint32_t *out16        = malloc(16 * sizeof(uint32_t));
    assert(out16 != NULL);
    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
        // we want the first 32 bytes
        memcpy(message_words + 0, (&parent_chaining_values[2 * i + 0]), 32);
        memcpy(message_words + 8, (&parent_chaining_values[2 * i + 1]), 32);

        if (current_number_of_nodes <= 2) flags |= ROOT;
        compress(IV, message_words, counter_t, 64, flags, out16);
        for (int j = 0; j < 8; j++) buffer_a[i][j] = out16[j];
    }
    free(out16);
    current_number_of_nodes >>= 1;
    if (current_number_of_nodes == 1) myprintf("skipping while loop\n");

    uint8_t a_or_b = 1;
    while (current_number_of_nodes > 1) {
        if (current_number_of_nodes <= 2) flags |= ROOT;

        for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
            if (a_or_b) {
                memcpy(message_words + 0, &buffer_a[2 * i][0], 32);
                memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
            } else {
                memcpy(message_words + 0, &buffer_b[2 * i][0], 32);
                memcpy(message_words + 8, &buffer_b[2 * i + 1][0], 32);
            }

            uint32_t out16[16];
            compress(IV, message_words, counter_t, 64, flags, out16);

            memcpy((a_or_b) ? &buffer_b[i] : &buffer_a[i], out16, sizeof(out16) >> 1);
        }
        current_number_of_nodes >>= 1;
        a_or_b = !a_or_b;
    }
    free(buffer_a);
    free(buffer_b);
    if (current_number_of_nodes != 1)
        myprintf("current_number_of_nodes: %d\n", current_number_of_nodes);
    assert(current_number_of_nodes == 1);

    counter_t--;  // always -1
    write_output(message_words, counter_t, flags, output, output_len);

    if (has_key || derive_key_context != NULL) free(IV);
}

#if defined MYBLAKE_MAIN
int         main(void) {
    char    filename[] = "input_data/input_256MB.txt";
    size_t  output_len = 128;
    uint8_t output[output_len];

    myblake(filename, output, output_len);

    printf("\n\nOutput: ");
    for (size_t i = 0; i < output_len; i++) {
        if (i % 32 == 0) printf("\n");
        printf("%02x", output[i]);
    }
    printf("\n");
    return 0;
}
#endif
