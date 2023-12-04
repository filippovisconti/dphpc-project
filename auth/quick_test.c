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
    printf("************** REFERENCE BLAKE3 STOUT **************\n");
    blake3(has_key, key, derive_key_context, output_len, input, output_ref);
    fclose(input);
    printf("************** MY BLAKE3 STOUT *********************\n");
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
    char  prefix[] = "input_data/input_";
    char  suffix[] = ".txt";
    char *sizes[]  = {"64_B", "128_B", "156_B", "512_B", "1KB", "2KB", "3KB",
         "4KB",  // "5KB", "6KB",
         /* "7KB", */ "8KB", "32KB", "64KB",
         /*  "129KB",  */ "256KB", "512KB", "1MB", "2MB", "4MB", "16MB", "32MB", "64MB", "128MB",
         "256MB", "512MB", "1GB"};  //, "2GB", "4GB", "8GB"};

    size_t num_sizes         = 20;
    int    num_avail_threads = omp_get_max_threads();
    omp_set_dynamic(0);
    for (int mode = 0; mode < 3; mode++)
        for (size_t size = 0; size < num_sizes; size++) {
            char filename[100];
            sprintf(filename, "%s%s%s", prefix, sizes[size], suffix);
            printf("[INFO:] Running test for %26s: ", filename);
            omp_set_num_threads(num_avail_threads);  // e
            run_test(filename, mode);
        }

    return EXIT_SUCCESS;
}
