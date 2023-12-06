#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "myblake.h"
#include "reference_impl.h"

void run_test(char *filename, int mode) {
    size_t output_len = BLAKE3_OUT_LEN * 4;

    uint8_t key[BLAKE3_KEY_LEN];
    if (mode == 1)
        for (size_t i = 0; i < BLAKE3_KEY_LEN; i++) key[i] = i;
    bool has_key = (mode == 1) ? true : false;

    char *derive_key_context = NULL;
    if (mode == 2) {
        derive_key_context = malloc(65);
        for (size_t i = 0; i < 64; i++) derive_key_context[i] = i + 3;
        derive_key_context[64] = '\0';
    }
    // FILE *input = fopen("test_input", "rb");
    FILE *input = fopen(filename, "rb");

    if (input == NULL) {
        printf("Error opening file\n");
        exit(1);
    }

    uint8_t *output_ref = malloc(output_len);
    uint8_t *output_my  = malloc(output_len);
    myprintf("************** REFERENCE BLAKE3 STOUT **************\n");
    blake3(has_key, key, derive_key_context, output_len, input, output_ref);
    fclose(input);
    myprintf("************** MY BLAKE3 STOUT *********************\n");
    myblake(filename, output_my, output_len, has_key, key, derive_key_context, 0);
    // printf("****************************************************\n");
    free(derive_key_context);
    char *output_hex_ref = malloc(output_len * 2 + 1);
    char *output_hex     = malloc(output_len * 2 + 1);
    for (size_t i = 0; i < output_len; i++) {
        sprintf(output_hex_ref + 2 * i, "%02x", output_ref[i]);
        sprintf(output_hex + 2 * i, "%02x", output_my[i]);
    }

    //=========================================
    bool cond = strcmp(output_hex, output_hex_ref) == 0;
    if (!cond) {
        printf("[REFERENCE]:\n");
        PRINT_WITH_NEWLINE(output_hex_ref, 64);
        printf("\n");
        printf("[CALCULATED]:\n");
        PRINT_WITH_NEWLINE(output_hex, 64);
        printf("\n");
        // for (size_t i = 0; i < output_len; i++) {
        //     if (output_ref[i] != output_my[i]) {
        //         printf("ERROR: %3zu | ", i);
        //         printf("REF: %02x | ", output_ref[i]);
        //         printf("GOT: %02x\n", output_my[i]);
        //     }
        // }
        free(output_ref);
        free(output_my);
        free(output_hex_ref);
        free(output_hex);
        exit(1);
    }
    assert(cond);
    printf("SUCCESS\n");
    free(output_ref);
    free(output_my);
    free(output_hex_ref);
    free(output_hex);
}

int main(void) {
    char prefix[] = "input_data/input_";
    char suffix[] = ".txt";
    char sizes[50][10];  // Assuming a maximum of 50 entries with a maximum size of 10 characters

    // Read sizes from file
    FILE *sizesFile = fopen("input_sizes.txt", "r");
    assert(sizesFile != NULL);

    size_t num_sizes = 0;
    while (num_sizes < 50 && fscanf(sizesFile, "%9s", sizes[num_sizes]) == 1) {
        // Skip lines starting with '#'
        if (sizes[num_sizes][0] == '#') {
            // Read and discard the rest of the line
            while (fgetc(sizesFile) != '\n' && !feof(sizesFile)) {
                // Do nothing, just consume characters until the end of the line
            }
        } else num_sizes++;
    }

    fclose(sizesFile);

    int num_avail_threads = omp_get_max_threads();
    omp_set_dynamic(0);
    for (int mode = 0; mode < 3; mode++)
        for (size_t size = 0; size < num_sizes; size++) {
            char *filename = malloc(strlen(prefix) + strlen(sizes[size]) + strlen(suffix) + 1);
            assert(filename != NULL);

            strcpy(filename, prefix);
            strcat(filename, sizes[size]);
            strcat(filename, suffix);
            // Check if the file exists
            if (access(filename, F_OK) == -1) {
                printf("[INFO:] Skipping test for %25s (File not found)\n", filename);
                continue;
            }
            printf("[INFO:] Running test for %26s: ", filename);
            omp_set_num_threads(num_avail_threads);  // e
            run_test(filename, mode);
            free(filename);
        }

    return EXIT_SUCCESS;
}
