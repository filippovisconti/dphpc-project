#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <errno.h>
#include "omp.h"

#include "blake3.h"
#include "include/reference_impl.h"
#include <papi.h>

void blake3_baseline(char* test_file, bool has_key, const uint8_t key[BLAKE3_KEY_LEN], const char* key_context, uint8_t* output);
bool output_cmp(uint8_t arr1[], uint8_t arr2[]) {
    for (size_t i = 0; i < BLAKE3_OUT_LEN; i++) {
        if (arr1[i] != arr2[i]) return false;
    }
    return true;
}

int main(int argc, char *argv[]) {
    if (argc <= 1) return 1;
    char* test_file = argv[1];
    if (argc == 3) omp_set_num_threads(atoi(argv[2]));


    PAPI_library_init(PAPI_VER_CURRENT);
    int eventset=PAPI_NULL;
    PAPI_create_eventset(&eventset);
    PAPI_add_event(event_set, PAPI_TOT_CYC);  // Total cycles
    PAPI_add_event(event_set, PAPI_TOT_INS);  // Total instructions
    long long count[2];

    uint8_t verified_output[BLAKE3_OUT_LEN];
    uint8_t output[BLAKE3_OUT_LEN];

    printf("Standard Mode\n");
    // base mode - baseline
    PAPI_start(eventset);
    blake3_baseline(test_file, false, NULL, NULL, verified_output);
    PAPI_stop(eventset, count);
    printf("    BASELINE -- cycles: %li     instrs: %li\n", count[0], count[1]);

    // base mode - single thread
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3(test_file, false, NULL, NULL, output, SINGLE_THREAD);
    PAPI_stop(eventset,count);
    assert(output_cmp(verified_output, output) == true);
    printf("    SINGLE_THREAD -- cycles: %li     instrs: %li\n", count[0], count[1]);

    // base mode - multi thread
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3(test_file, false, NULL, NULL, output, MULTI_THREAD);
    PAPI_stop(eventset, count);
    assert(output_cmp(verified_output, output) == true);
    printf("    MULTI_THREAD -- cycles: %li     instrs: %li\n", count[0], count[1]);

    // -------------------------------------------
    printf("Keyed Mode\n");
    uint8_t key[BLAKE3_KEY_LEN];
    for (int i = 0; i < BLAKE3_KEY_LEN; i++) key[i] = i;

    // keyed mode - baseline
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3_baseline(test_file, true, key, NULL, verified_output);
    PAPI_stop(eventset, count);
    printf("    BASELINE -- cycles: %li     instrs: %li\n", count[0], count[1]);

    // keyed mode - single thread
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3(test_file, true, key, NULL, output, SINGLE_THREAD);
    PAPI_stop(eventset, count);
    printf("    SINGLE_THREAD -- cycles: %li     instrs: %li\n", count[0], count[1]);
    assert(output_cmp(verified_output, output) == true);

    // keyed mode - multi thread
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3(test_file, true, key, NULL, output, MULTI_THREAD);
    PAPI_stop(eventset, count);
    printf("    MULTI_THREAD -- cycles: %li     instrs: %li\n", count[0], count[1]);
    assert(output_cmp(verified_output, output) == true);

    // -------------------------------------------
    printf("Derive Key Mode\n");
    char context[2048];
    for (int i = 0; i < 2048; i++) context[i] = 'A' + (i % 26);

    // derive key mode - baseline
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3_baseline(test_file, false, NULL, context, verified_output);
    PAPI_stop(eventset, count);
    printf("    BASELINE -- cycles: %li     instrs: %li\n", count[0], count[1]);
    // for (size_t i = 0; i < BLAKE3_OUT_LEN / sizeof(uint8_t); i++) 
    //     printf("%02x", verified_output[i]);
    // printf("\n");

    // derive key mode - single thread
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3(test_file, false, NULL, context, output, SINGLE_THREAD);
    PAPI_stop(eventset, count);
    printf("    SINGLE_THREAD -- cycles: %li     instrs: %li\n", count[0], count[1]);
    assert(output_cmp(verified_output, output) == true);

    // derive key mode - multi thread
    PAPI_reset(eventset);
    PAPI_start(eventset);
    blake3(test_file, false, NULL, context, output, MULTI_THREAD);
    PAPI_stop(eventset, count);
    printf("    MULTI_THREAD -- cycles: %li     instrs: %li\n", count[0], count[1]);
    assert(output_cmp(verified_output, output) == true);

    return 0;
}

void blake3_baseline(char* test_file, bool has_key, const uint8_t key[BLAKE3_KEY_LEN], const char* key_context, uint8_t* output) {
    FILE *input_stream = fopen(test_file, "rb");
    if (input_stream == NULL) {
        printf("Can't open file\n");
        exit(1);
    }

    blake3_hasher hasher;
    if (has_key) {
        blake3_hasher_init_keyed(&hasher, key);
    } else if (key_context != NULL) {
        blake3_hasher_init_derive_key(&hasher, key_context);
    } else {
        blake3_hasher_init(&hasher);
    }

    unsigned char buf[65536];

    while (1) {
        ssize_t n = fread(buf, 1, sizeof(buf), input_stream);
        if (n > 0) {
            blake3_hasher_update(&hasher, buf, n);
        } else if (n == 0) {
            break; // end of file
        } else {
            fprintf(stderr, "read failed: %s\n", strerror(errno));
            exit(1);
        }
    }

    fclose(input_stream);
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
}