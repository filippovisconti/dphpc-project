
#include <stdio.h>
#include <stdlib.h>

#include "myblake.h"
#include "reference_impl.h"
#include "sha256.h"

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
    uint8_t     key[BLAKE3_KEY_LEN] = {0};
    bool        has_key             = false;
    const char *derive_key_context  = NULL;
    uint8_t    *output              = malloc(BLAKE3_OUT_LEN);

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
    myblake(filename, output, BLAKE3_OUT_LEN);
    PAPI_REGION_END("my_blake3");
    printf("[INFO:] MY  BLAKE3 done\n");

    free(output);
}

int main(void) {
    endianCheckPrint();
    char prefix[] = "input_data/input_";
    char suffix[] = ".txt";

    size_t num_sizes = 14;
    char  *sizes[]   = {"8KB", "32KB", "64KB", /*  "128KB", */ "512KB", "1MB", "2MB", "4MB", "16MB",
           "32MB", "64MB", "128MB", "256MB", "512MB", "1GB"};  //, "4GB", "8GB"};
    int    repetitions = 15;

    int num_avail_threads = omp_get_max_threads();
    printf("[INFO:] Max number of threads: %d\n", num_avail_threads);
    omp_set_dynamic(0);

    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < repetitions; i++) {
            char filename[100];
            sprintf(filename, "%s%s%s", prefix, sizes[size], suffix);
            printf("[INFO:] Running benchmark for %s, %d\n", sizes[size], i);
            omp_set_num_threads(num_avail_threads);
            run_benchmark_my(filename);
        }
    }
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < repetitions; i++) {
            char filename[100];
            sprintf(filename, "%s%s%s", prefix, sizes[size], suffix);
            printf("[INFO:] Running ref benchmark for %s, %d\n", sizes[size], i);
            run_benchmark_ref(filename);
        }
    }
    for (size_t size = 0; size < num_sizes; size++) {
        for (int i = 0; i < repetitions; i++) {
            char filename[100];
            sprintf(filename, "%s%s%s", prefix, sizes[size], suffix);
            printf("[INFO:] Running sha benchmark for %s, %d\n", sizes[size], i);
            run_benchmark_sha(filename);
        }
    }
    return 0;
}
