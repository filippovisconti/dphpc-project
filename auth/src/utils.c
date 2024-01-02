#include <stdint.h>

#include "blake3_impl.h"
#include "blake_f.h"

static uint32_t base_IV[8] = {
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
void compress(const uint32_t chaining_value[8], const uint32_t block_words[16], uint64_t counter,
    uint32_t block_len, uint32_t flags, uint32_t out[16]) {
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
        base_IV[0],
        base_IV[1],
        base_IV[2],
        base_IV[3],
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
    printf("flags:   %08x, counter %lld\n", flags, counter);
    printf("Ch val:  ");
    for (size_t i = 0; i < 8; i++) printf("%08x ", chaining_value[i]);
    printf("\n");
    printf("Block: %d \n", block_len);
    for (size_t i = 0; i < 8; i++) printf("%08x ", block_words[i]);
    printf("\n");
    for (size_t i = 8; i < 16; i++) printf("%08x ", block_words[i]);
    printf("\n");
    // printf("IV: ");
    // for (size_t i = 0; i < 4; i++) printf("%08x ", IV[i]);
    // printf("\n");
    printf("Out:     ");
    for (size_t i = 0; i < 8; i++) printf("%08x ", out[i]);
    printf("\n-------\n");
#endif
}

void compress_opt(const uint32_t chaining_value[8], const uint32_t block_words[],
    const uint32_t block_words2[], uint64_t counter, uint32_t block_len, uint32_t flags,
    uint32_t out[8]) {
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
        base_IV[0],
        base_IV[1],
        base_IV[2],
        base_IV[3],
        (uint32_t)counter,
        (uint32_t)(counter >> 32),
        block_len,
        flags,
    };
    uint32_t block[16];
    memcpy(block, block_words, sizeof(block) >> 1);
    memcpy(block + 8, block_words2, sizeof(block) >> 1);

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
    memcpy(out, state, 8 * sizeof(uint32_t));
}

void words_from_little_endian_bytes(const void *bytes, size_t bytes_len, uint32_t *out) {
    assert(bytes_len % 4 == 0);
    const uint8_t *u8_ptr = (const uint8_t *)bytes;
    for (size_t i = 0; i < (bytes_len / 4); i++) {
        out[i] = ((uint32_t)(*u8_ptr++));
        out[i] += ((uint32_t)(*u8_ptr++)) << 8;
        out[i] += ((uint32_t)(*u8_ptr++)) << 16;
        out[i] += ((uint32_t)(*u8_ptr++)) << 24;
    }
}

int next_multiple_of_64(int n) {
    return (n + 63) & ~63;
}

int get_num_chunks(char *filename) {
    myprintf("using %s\n", filename);
    FILE *input = fopen(filename, "rb");
    assert(input != NULL);
    fseek(input, 0L, SEEK_END);
    uint64_t size = ftell(input);
    fclose(input);

    // myprintf("input size: %lld\n", size);
    int  num_chunks        = size >> CHUNK_SIZE_LOG;  // divide by 1024
    bool chunks_pow_of_two = (num_chunks & (num_chunks - 1)) == 0;

    if (!chunks_pow_of_two) exit(0);
    return MAX(num_chunks, 1);
}

void write_output(uint32_t *int_IV, uint32_t message_words[16], uint32_t counter_t, uint32_t flags,
    uint8_t *output, size_t output_len) {
    uint8_t *running_output     = output;
    size_t   running_output_len = output_len;
    assert(output != NULL);
    memset(output, 0, output_len);

    bool stop = false;
    while (running_output_len > 0) {
        uint32_t words[16];
        myprintf("write_output\n");
        compress(int_IV, message_words, ++counter_t, 64, flags, words);

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

int set_num_threads(int num_chunks) {
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

    myprintf("[OMP] Number of usable threads: %d\n", omp_get_max_threads());
    assert(omp_get_max_threads() == num_threads);
    myprintf("[OMP] Running with num_threads: %d\n", num_threads);
#else
    (void)num_chunks;
    int num_threads = 2;
#endif
    return num_threads;
}

void compute_chunk_chaining_values(uint32_t *int_IV, char *read_buffer, int num_blocks,
    uint64_t counter_t, int chunk, uint32_t chaining_value[8], uint32_t base_flags) {

    for (int block = 0; block < num_blocks; block++) {
        uint32_t output_blocks[16];
        int      access_index = (chunk % 4) * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
        words_from_little_endian_bytes(&read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

        uint32_t flags = base_flags;
        if (block == 0) flags |= CHUNK_START;
        if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;

        if (num_blocks == 1) assert(flags == (CHUNK_START | CHUNK_END));

        const uint32_t *input_chaining_value = (block == 0) ? int_IV : chaining_value;
#ifndef NO_VECTORIZE
        blake3_compress_xof_sse41_opt(input_chaining_value, (uint8_t *)output_blocks,
            (uint8_t *)(output_blocks + 8), BLAKE3_BLOCK_LEN, counter_t, flags,
            (uint8_t *)chaining_value);
#else
        compress_opt(input_chaining_value, output_blocks, output_blocks + 8, counter_t,
            BLAKE3_BLOCK_LEN, flags, chaining_value);
#endif
    }
}
