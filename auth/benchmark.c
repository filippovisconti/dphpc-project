
#include <stdio.h>
#include <stdlib.h>

#include "reference_impl.h"
#include "sha256.h"
#ifdef __x86_64__
#include <papi.h>
#endif

#ifdef _PAPI
#define PAPI_REGION_BEGIN(name)                                                \
    do {                                                                       \
        int retval = PAPI_hl_region_begin(name);                               \
        if (retval != PAPI_OK) handle_error(1);                                \
    } while (0)

#define PAPI_REGION_END(name)                                                  \
    do {                                                                       \
        int retval = PAPI_hl_region_end(name);                                 \
        if (retval != PAPI_OK) handle_error(1);                                \
    } while (0)

void handle_error(int retval) {
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
}
#else
#define PAPI_REGION_BEGIN(name)                                                \
    // printf("PAPI not enabled - region %s start\n", name);

#define PAPI_REGION_END(name)                                                  \
    // printf("PAPI not enabled - region %s end\n", name);

void handle_error() {
    exit(1);
}
#endif

void run_benchmark(char *filename) {
#ifdef _PAPI
    int retval;
    (void)retval;
#endif

    // FILE *fileForPrinting;
    FILE *file = NULL;

    // Open a file, specifying which file using command line arguments
    file = fopen(filename, "rb");

    // First check to make sure the file could be found
    if (file == NULL) {
        printf("\n[ERROR:] Could not open file %s\n", filename);
        exit(1);
    }

    // Function calls
    printf("[INFO:] File ok, executing functions.");

    // fileForPrinting = fopen(fileName, "r");
    // printFileContents(fileForPrinting);

    PAPI_REGION_BEGIN("sha256");
    calculate_sha256(file);
    PAPI_REGION_END("sha256");

    file = fopen(filename, "r");

    if (file == NULL) {
        printf("\n[ERROR:] Could not open file %s\n", filename);
        exit(1);
    }

    uint8_t     key[BLAKE3_KEY_LEN] = {0};
    bool        has_key             = false;
    const char *derive_key_context  = NULL;
    size_t      output_len          = BLAKE3_OUT_LEN;

    // printf("[INFO:] Starting BLAKE3 algorithm.. \n");
    printf(
        "=================== BLAKE3 HASH OUTPUT "
        "==================================\n");
    PAPI_REGION_BEGIN("blake3");
    blake3(has_key, key, derive_key_context, output_len, file);
    PAPI_REGION_END("blake3");
    printf(
        "======================================================================"
        "==="
        "\n\n");

    fclose(file);
}

int main(void) {
    endianCheckPrint();
    char prefix[] = "input_data/input_";
    char suffix[] = ".txt";

    char *sizes[] = {
        "1KB", "10KB", "100KB", "1MB", "10MB", "100MB"};  //, "1GB"};
    size_t num_sizes = 6;

    for (size_t size = 0; size < num_sizes; size++) {
        char filename[100];
        sprintf(filename, "%s%s%s", prefix, sizes[size], suffix);
        printf("[INFO:] Running benchmark for %s\n", filename);
        run_benchmark(filename);
    }

    return 0;
}
