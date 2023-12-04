#include "myblake.h"

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

uint32_t *IV = base_IV;

static void short_inputs(
    char *filename, uint8_t *output, size_t output_len, int num_chunks, uint32_t base_flags) {

    FILE *input = fopen(filename, "rb");
    assert(input != NULL);

    uint32_t(*chunk_chaining_values)[8] = calloc(num_chunks, sizeof(uint32_t[8]));
    assert(chunk_chaining_values != NULL);

    uint32_t output_blocks[16];
    uint32_t save_flags = base_flags;
    uint32_t save_chaining_value[8];  // store output here
    memcpy(save_chaining_value, IV, sizeof(save_chaining_value));

    char  *read_buffer = calloc(8, CHUNK_SIZE);
    size_t len         = fread(read_buffer, 1, 8 * CHUNK_SIZE, input);
    assert(len != 0);

    // if ((len & 63) != 0) len = next_multiple_of_64(len);
    // assert((len & 63) == 0);

    int num_read_chunks = CEIL_DIV(len, CHUNK_SIZE);
    assert(num_read_chunks <= 8);

    int chunk = 0, chunk_counter = 0;
    int remaining = -1;
    // START BASE CHUNK PROCESSING
    for (int i = 0; i < num_read_chunks; i++, chunk++) {
        int num_blocks = 16;
        remaining      = (len - CHUNK_SIZE * (num_read_chunks - 1));  // 0 <= remainder <= 1024
        myprintf("\tremaining: %d\n", remaining);
        if (chunk == (num_read_chunks - 1)) {
            num_blocks = CEIL_DIV(remaining, BLAKE3_BLOCK_LEN);
            myprintf("num_blocks: %d\n", num_blocks);
        }

        uint64_t counter_t = chunk;
        uint32_t chaining_value[8];  // store output here
        for (int block = 0; block < num_blocks; block++) {
            int access_index = chunk * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
            words_from_little_endian_bytes(
                &read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

            myprintf("\tremaining: %d\n", remaining);
            uint32_t flags = base_flags;
            if (block == 0) flags |= CHUNK_START;
            if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;
            if (num_chunks == 1 && (block == num_blocks - 1)) flags |= ROOT;
            if (num_chunks == 1) {
                save_flags = flags;
                if (num_blocks != 1)
                    memcpy(save_chaining_value, chaining_value, sizeof(chaining_value));
            }
            uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);
            uint32_t  out16[16];
            compress(input_chaining_value, output_blocks, counter_t,
                remaining > 64 ? BLAKE3_BLOCK_LEN : remaining, flags, out16);

            memcpy(chaining_value, out16, sizeof(out16) >> 1);
            if (remaining >= 64) { remaining -= BLAKE3_BLOCK_LEN; }
            if (num_blocks == 1) {
                assert((flags & (CHUNK_START | CHUNK_END)) == (CHUNK_START | CHUNK_END));
            }
        }
        assert(chunk_counter <= ((int)num_chunks - 1));
        for (int j = 0; j < 8; j++) chunk_chaining_values[chunk_counter][j] = chaining_value[j];
        chunk_counter++;
        myprintf("chunk_counter: %d\n", chunk_counter);
    }
    free(read_buffer);

    if (remaining == 0) remaining = BLAKE3_BLOCK_LEN;

    if (num_chunks == 1) {
        assert(output != NULL);
        memset(output, 0, output_len);

        myprintf("num_chunks == 1\n");
        base_flags |= ROOT;
        int counter_t = -1;

        myprintf("output_len: %ld\n", output_len);
        uint8_t *running_output     = output;
        size_t   running_output_len = output_len;

        bool stop = false;
        while (running_output_len > 0) {
            uint32_t words[16];
            compress(save_chaining_value, output_blocks, ++counter_t,
                remaining, save_flags, words);

            stop = false;
            for (size_t word = 0; word < 16 && !stop; word++) {
                for (int byte = 0; byte < 4; byte++) {
                    if (output_len == 0 || running_output_len == 0) {
                        myprintf("\nFINISHED\n");
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
    int current_number_of_nodes = num_chunks;

    uint32_t message_words[16];  // first 8 is left child, second 8 is the right one;
    uint32_t(*buffer_a)[8] = (uint32_t(*)[8])calloc(num_chunks / 2, sizeof(uint32_t[8]));
    uint32_t(*buffer_b)[8] = (uint32_t(*)[8])calloc(num_chunks / 4, sizeof(uint32_t[8]));
    uint32_t *out16        = malloc(16 * sizeof(uint32_t));

    assert(buffer_a != NULL);
    assert(buffer_b != NULL);
    assert(out16 != NULL);

    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
        memcpy(message_words + 0, (&chunk_chaining_values[2 * i + 0]), 32);
        memcpy(message_words + 8, (&chunk_chaining_values[2 * i + 1]), 32);

        base_flags |= PARENT;
        if (current_number_of_nodes <= 2) base_flags |= ROOT;
        compress(IV, message_words, 0, 64, base_flags, out16);

        for (int j = 0; j < 8; j++) buffer_a[i][j] = out16[j];
    }

    current_number_of_nodes >>= 1;
    free(chunk_chaining_values);
    if (current_number_of_nodes == 1)
        write_output(IV, buffer_a[0], 0, base_flags, output, output_len);
    else {
        uint8_t a_or_b = 1;
        while (current_number_of_nodes > 1) {
            if (current_number_of_nodes <= 2) base_flags |= ROOT;
            int do_later = current_number_of_nodes & 1;
            for (int i = 0; i < ((current_number_of_nodes - do_later) >> 1); i++) {
                if (a_or_b) {
                    assert(2 * i + 1 < num_chunks / 2);
                    memcpy(message_words + 0, &buffer_a[2 * i][0], 32);
                    memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
                } else {
                    assert(2 * i + 1 < num_chunks / 4);
                    memcpy(message_words + 0, &buffer_b[2 * i][0], 32);
                    memcpy(message_words + 8, &buffer_b[2 * i + 1][0], 32);
                }

                compress(IV, message_words, 0, 64, base_flags, out16);
                memcpy((a_or_b) ? &buffer_b[i] : &buffer_a[i], out16, sizeof(out16) >> 1);
            }
            current_number_of_nodes >>= 1 + do_later;
            a_or_b = !a_or_b;
        }
    }
    free(out16);
    free(buffer_a);
    free(buffer_b);

    assert(current_number_of_nodes == 1);
    int counter_t = -1;
    write_output(IV, message_words, counter_t, base_flags, output, output_len);
}

void myblake(char *filename, uint8_t *output, size_t output_len, bool has_key, uint8_t *key,
    const char *derive_key_context, uint32_t initial_flags) {

    IV = base_IV;

    uint32_t base_flags = initial_flags;

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
        fwrite(derive_key_context, strlen(derive_key_context), 1, f);  // write context to file
        fclose(f);

        uint8_t *context_key_words = malloc(BLAKE3_KEY_LEN);
        myblake(fn, context_key_words, BLAKE3_KEY_LEN, false, NULL, NULL,
            DERIVE_KEY_CONTEXT);  // hash context
        IV = malloc(8 * sizeof(uint32_t));
        assert(IV != NULL);
        words_from_little_endian_bytes(context_key_words, BLAKE3_KEY_LEN, IV);
        free(context_key_words);
        base_flags = DERIVE_KEY_MATERIAL;
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
                    IV, read_buffer, num_blocks, counter_t, chunk, chaining_value, base_flags);

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
    write_output(IV, message_words, counter_t, flags, output, output_len);

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
