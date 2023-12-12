
#include <stdio.h>
#include <stdlib.h>

#include "myblake.h"
#include "reference_impl.h"
#include "sha256.h"

#define REPETITIONS 15
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
#define PAPI_REGION_BEGIN(name) printf("PAPI not enabled - region %s start\n", name);

#define PAPI_REGION_END(name)   printf("PAPI not enabled - region %s end\n", name);

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
    printf("[INFO:] SHA256     done\n");
    fclose(file);
}
void run_benchmark_ref(char *filename) {
    FILE *file = fopen(filename, "rb");
    assert(file != NULL);
    uint8_t *output = malloc(BLAKE3_OUT_LEN);

    // printf("[INFO:] Starting ref BLAKE3 algorithm.. \n");
    file = fopen(filename, "r");
    assert(file != NULL);
    PAPI_REGION_BEGIN("ref_blake3");
    blake3(has_key, key, derive_key_context, BLAKE3_OUT_LEN, file, output);
    PAPI_REGION_END("ref_blake3");
    fclose(file);
    printf("[INFO:] REF BLAKE3 done\n");
    free(output);
}
void run_benchmark_my(char *filename) {
    uint8_t *output = malloc(BLAKE3_OUT_LEN);
    // printf("[INFO:] Starting my BLAKE3 algorithm.. \n");
    PAPI_REGION_BEGIN("my_blake3");
    myblake(filename, output, BLAKE3_OUT_LEN, has_key, key, derive_key_context, 0);
    PAPI_REGION_END("my_blake3");
    printf("[INFO:] MY  BLAKE3 done\n");

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

    int   str_len  = strlen(prefix) + 8 * sizeof(char) + strlen(suffix) + 1;
    char *filename = malloc(str_len);
    assert(filename != NULL);

#ifdef USE_OPENMP
    int num_avail_threads = omp_get_max_threads();
    omp_set_dynamic(0);
    printf("[INFO:] Max number of threads: %d\n", num_avail_threads);
    for (size_t size = 0; size < num_sizes; size++) {
        memset(filename, 0, str_len);
        strcpy(filename, prefix);
        strcat(filename, sizes[size]);
        strcat(filename, suffix);

        for (int i = 0; i < REPETITIONS; i++) {
            printf("[INFO:] Running benchmark for %s, %d\n", sizes[size], i);
            omp_set_num_threads(num_avail_threads);
            run_benchmark_my(filename);
        }
    }
#else
    for (size_t size = 0; size < num_sizes; size++) {
        memset(filename, 0, str_len);
        strcpy(filename, prefix);
        strcat(filename, sizes[size]);
        strcat(filename, suffix);
        for (int i = 0; i < REPETITIONS; i++) {
            printf("[INFO:] Running benchmark for %s, %d\n", sizes[size], i);
            run_benchmark_my(filename);
        }
    }
    for (size_t size = 0; size < num_sizes; size++) {
        memset(filename, 0, str_len);
        strcpy(filename, prefix);
        strcat(filename, sizes[size]);
        strcat(filename, suffix);

        for (int i = 0; i < REPETITIONS; i++) {
            printf("[INFO:] Running ref benchmark for %s, %d\n", sizes[size], i);
            run_benchmark_ref(filename);
        }
    }
    for (size_t size = 0; size < num_sizes; size++) {
        memset(filename, 0, str_len);
        strcpy(filename, prefix);
        strcat(filename, sizes[size]);
        strcat(filename, suffix);

        for (int i = 0; i < REPETITIONS; i++) {
            printf("[INFO:] Running sha benchmark for %s, %d\n", sizes[size], i);
            run_benchmark_sha(filename);
        }
    }
#endif
    free(filename);
    return 0;
}
