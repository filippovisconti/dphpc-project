
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "blake_d.h"
#include "blake_f.h"
#include "reference_impl.h"
#include "sha256.h"

#define REPETITIONS 15
#define OUTPUT_LEN  256

#ifdef USE_PAPI
#include <papi.h>
#endif

#ifdef _PAPI
#define PAPI_REGION_BEGIN(name)                                                                    \
    do {                                                                                           \
        int retval = PAPI_hl_region_begin(name);                                                   \
        if (retval != PAPI_OK) handle_error(1);                                                    \
    } while (0)

#define PAPI_REGION_END(name)                                                                      \
    do {                                                                                           \
        int retval = PAPI_hl_region_end(name);                                                     \
        if (retval != PAPI_OK) handle_error(1);                                                    \
    } while (0)

void handle_error(int retval) {
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}
#else
#define PAPI_REGION_BEGIN(name) (void)0;  // printf("PAPI not enabled - region %s start\n", name);

#define PAPI_REGION_END(name)   (void)0;  // printf("PAPI not enabled - region %s end\n", name);

void handle_error() {
    exit(1);
}
#endif

uint8_t     key[BLAKE3_KEY_LEN] = {0};
bool        has_key             = false;
const char *derive_key_context  = NULL;

void run_benchmark_sha(char *filename) {
    FILE *file = fopen(filename, "rb");
    assert(file != NULL);

    PAPI_REGION_BEGIN("sha256");
    calculate_sha256(file);
    PAPI_REGION_END("sha256");

    fclose(file);
}

void run_benchmark_ref(char *filename) {
    FILE *file = fopen(filename, "r");
    assert(file != NULL);
    uint8_t *output = malloc(OUTPUT_LEN);
    assert(output != NULL);

    PAPI_REGION_BEGIN("ref_blake3");
    blake3(has_key, key, derive_key_context, OUTPUT_LEN, file, output);
    PAPI_REGION_END("ref_blake3");

    fclose(file);
    free(output);
}

void run_benchmark_f(char *filename) {
    uint8_t *output = malloc(OUTPUT_LEN);
    assert(output != NULL);

    PAPI_REGION_BEGIN("blake3_f");
    myblake(filename, output, OUTPUT_LEN, has_key, key, derive_key_context, 0);
    PAPI_REGION_END("blake3_f");

    free(output);
}

void run_benchmark_d(char *filename) {
    uint8_t *output = malloc(OUTPUT_LEN);
    PAPI_REGION_BEGIN("blake3_d");
    blake(filename, false, NULL, NULL, output, OUTPUT_LEN,
#ifdef USE_OPENMP
        MULTI_THREAD);
#else
        SINGLE_THREAD);
#endif
    PAPI_REGION_END("blake3_d");

    free(output);
}

int main(void) {
    endianCheckPrint();
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

    int    str_len  = strlen(prefix) + 8 * sizeof(char) + strlen(suffix) + 1;
    char **filename = malloc(num_sizes * sizeof(char *));
    for (size_t i = 0; i < num_sizes; i++) {
        filename[i] = malloc(str_len * sizeof(char));
        assert(filename[i] != NULL);
    }

    assert(filename != NULL);

    for (size_t size = 0; size < num_sizes; size++) {
        memset(filename[size], 0, str_len);
        strcpy(filename[size], prefix);
        strcat(filename[size], sizes[size]);
        strcat(filename[size], suffix);
    }

#ifdef USE_OPENMP
    int num_avail_threads = omp_get_max_threads();
    omp_set_dynamic(0);
    printf("[INFO:] Max number of threads: %d\n", num_avail_threads);
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running f_benchmark for %5s, %2d", sizes[size], i);
            omp_set_num_threads(num_avail_threads);
            // run_benchmark_f(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
    }

    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running d_benchmark for %5s, %2d", sizes[size], i);
            omp_set_num_threads(num_avail_threads);
            run_benchmark_d(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
    }
#else
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running f benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_f(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
    }

    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running d benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_d(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
    }

    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running ref benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_ref(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
    }
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running sha benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_sha(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
    }
#endif
    for (size_t i = 0; i < num_sizes; i++) free(filename[i]);
    free(filename);
    return 0;
}
