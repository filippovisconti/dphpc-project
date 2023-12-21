
#include <assert.h>
#include <stdint.h>
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
#elif defined(__x86_64__)
#include <x86intrin.h>
unsigned int ui;
uint64_t     start, end;
#define PAPI_REGION_BEGIN(name)                                                                    \
    do { start = __rdtscp(&ui); } while (0)

#define PAPI_REGION_END(name)                                                                      \
    do {                                                                                           \
        end = __rdtscp(&ui);                                                                       \
        fprintf(file, "%llu,", (long long)end - start);                                            \
        fflush(file);                                                                              \
    } while (0)
#else
#define PAPI_REGION_BEGIN(name) (void)0;
#define PAPI_REGION_END(name)   (void)0;

#endif

// ------ GLOBAL VARIABLES ------
FILE       *file;
uint8_t     key[BLAKE3_KEY_LEN] = {0};
bool        has_key             = false;
const char *derive_key_context  = NULL;
// ------------------------------

void run_benchmark_sha(char *filename) {
    FILE *sha_file = fopen(filename, "rb");
    assert(sha_file != NULL);

    PAPI_REGION_BEGIN("sha256");
    calculate_sha256(sha_file);
    PAPI_REGION_END("sha256");

    fclose(sha_file);
}

void run_benchmark_ref(char *filename) {
    FILE *ref_file = fopen(filename, "r");
    assert(ref_file != NULL);
    uint8_t *output = malloc(OUTPUT_LEN);
    assert(output != NULL);

    PAPI_REGION_BEGIN("ref_blake3");
    blake3(has_key, key, derive_key_context, OUTPUT_LEN, ref_file, output);
    PAPI_REGION_END("ref_blake3");

    fclose(ref_file);
    free(output);
}

void run_benchmark_f(char *filename) {
    uint8_t *output = malloc(OUTPUT_LEN);
    assert(output != NULL);

    PAPI_REGION_BEGIN("blake_f");
    myblake(filename, output, OUTPUT_LEN, has_key, key, derive_key_context, 0);
    PAPI_REGION_END("blake_f");

    free(output);
}

void run_benchmark_d(char *filename) {
    uint8_t *output = malloc(OUTPUT_LEN);
    assert(output != NULL);

    PAPI_REGION_BEGIN("blake_d");
    blake(filename, false, NULL, NULL, output, OUTPUT_LEN);
    PAPI_REGION_END("blake_d");

    free(output);
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

    // create output directory if not exists
#ifdef __x86_64__
    system("mkdir -p output_data");
#endif

    // Run benchmarks
#ifdef USE_OPENMP
    int tot_num_avail_threads = omp_get_max_threads();
    omp_set_dynamic(0);
    printf("[INFO:] total number of available threads: %d\n", tot_num_avail_threads);

    for (int num_threads = 2; num_threads <= tot_num_avail_threads; num_threads <<= 1) {
        char out_filename[50];
        sprintf(out_filename, "output_data/blake_f_%02d.csv", num_threads);
        file = fopen(out_filename, "w");

        printf("[INFO:] current number of threads: %d\n", num_threads);

        for (size_t size = 0; size < num_sizes; size++) {
            for (int i = 0; i < REPETITIONS; i++) {
                printf("\r[INFO:] Running f_benchmark for %5s, %2d", sizes[size], i);
                omp_set_num_threads(num_threads);
                run_benchmark_f(filename[size]);
                fflush(stdout);
            }
            printf(" done\n");
            fprintf(file, "\n");
        }
        fclose(file);
    }

    for (int num_threads = 2; num_threads <= tot_num_avail_threads; num_threads <<= 1) {
        char out_filename[50];
        sprintf(out_filename, "output_data/blake_d_%02d.csv", num_threads);
        file = fopen(out_filename, "w");

        printf("[INFO:] current number of threads: %d\n", num_threads);

        for (size_t size = 0; size < num_sizes; size++) {
            for (int i = 0; i < REPETITIONS; i++) {
                printf("\r[INFO:] Running d_benchmark for %5s, %2d", sizes[size], i);
                omp_set_num_threads(num_threads);
                run_benchmark_d(filename[size]);
                fflush(stdout);
            }
            printf(" done\n");
            fprintf(file, "\n");
        }
        fclose(file);
    }

#else
    file = fopen("output_data/blake_f.csv", "w");
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running f benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_f(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
        fprintf(file, "\n");
    }
    fclose(file);

    file = fopen("output_data/blake_d.csv", "w");
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running d benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_d(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
        fprintf(file, "\n");
    }
    fclose(file);

    file = fopen("output_data/blake3_ref.csv", "w");
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running ref benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_ref(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
        fprintf(file, "\n");
    }
    fclose(file);

    file = fopen("output_data/sha256.csv", "w");
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < REPETITIONS; i++) {
            printf("\r[INFO:] Running sha benchmark for %5s, %2d", sizes[size], i);
            run_benchmark_sha(filename[size]);
            fflush(stdout);
        }
        printf(" done\n");
        fprintf(file, "\n");
    }
    fclose(file);
#endif

    for (size_t i = 0; i < num_sizes; i++) free(filename[i]);
    free(filename);
    return 0;
}
