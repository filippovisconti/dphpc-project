#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blake_d.h"
#include "blake_f.h"

#ifndef BENCHMARK
#define BENCHMARK
#endif

#ifdef DEBUG
#undef DEBUG
#endif

// converts a hexadecimal character into a 4-bit integer
inline static uint8_t nibble_from_hex_char(char hex_char) {
    assert(('0' <= hex_char && hex_char <= '9') || ('a' <= hex_char && hex_char <= 'f'));
    if ('0' <= hex_char && hex_char <= '9') {
        return (uint8_t)(hex_char - '0');
    } else {
        return (uint8_t)(hex_char - 'a') + 10;
    }
}
// converts a hexadecimal string into a byte array
inline static void key_bytes_from_hex(const char *hex_key, uint8_t out[BLAKE3_KEY_LEN]) {
    assert(strlen(hex_key) == 2 * BLAKE3_KEY_LEN);
    for (size_t i = 0; i < BLAKE3_KEY_LEN; i++) {
        out[i] = nibble_from_hex_char(hex_key[2 * i]) * 16;
        out[i] += nibble_from_hex_char(hex_key[2 * i + 1]);
    }
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

    return chunks_pow_of_two;
}

int main(int argc, char **argv) {

#ifdef _WIN32
    printf("This code is not meant to run on Windows.\n");
    exit(0);
#endif
    uint8_t     key[BLAKE3_KEY_LEN];
    bool        has_key            = false;
    const char *derive_key_context = NULL;
    size_t      output_len         = BLAKE3_OUT_LEN;
    char        filename[50]       = {0};
    // This is a toy main function, and we don't bother to check for invalid
    // inputs like negative lengths here.
    while (argc > 1) {
        if (strcmp(argv[1], "--help") == 0) {
            printf(
                "Usage: blake3 [--len <BYTES>] [--key <HEX>] [--derive-key "
                "<CONTEXT>] [-f <file_name>] \n");
            return 0;
        } else if (strcmp(argv[1], "--len") == 0) {
            output_len = (size_t)strtoll(argv[2], NULL, 10);
            if (output_len <= 0) exit(1);
        } else if (strcmp(argv[1], "--key") == 0) {
            assert(derive_key_context == NULL);
            key_bytes_from_hex(argv[2], key);
            has_key = true;
        } else if (strcmp(argv[1], "--derive-key") == 0) {
            assert(!has_key);
            derive_key_context = argv[2];
        } else if (strcmp(argv[1], "-f") == 0) {
            strncpy(filename, argv[2], sizeof(filename));
        } else {
            printf("Unknown argument: %s\n", argv[1]);
            return 1;
        }
        argc -= 2;
        argv += 2;
    }

    uint8_t *output = malloc(output_len);

    if (get_num_chunks(filename)) {
        printf("************** FULL TREE **********************\n");
        myblake(filename, output, output_len, has_key, key, derive_key_context, 0);
    } else {
        printf("************** BIG  TREE **********************\n");
        blake(filename, has_key, key, derive_key_context, output, output_len);
    }
    char *output_hex = malloc(output_len * 2 + 1);
    for (size_t i = 0; i < output_len; i++) sprintf(output_hex + 2 * i, "%02x", output[i]);

    printf("\n[CALCULATED]:\n");
    PRINT_WITH_NEWLINE(output_hex, 64);

    free(output_hex);
    free(output);
    return 0;
}
