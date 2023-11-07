#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "reference_impl.h"

int main(void) {
    uint8_t     key[BLAKE3_KEY_LEN];
    bool        has_key            = false;
    const char *derive_key_context = NULL;
    size_t      output_len         = BLAKE3_OUT_LEN * 4;

    char result[] =
        "aae792484c8efe4f19e2ca7d371d8c467ffb10748d8a5a1ae579948f718a2a635fe51a"
        "27db045a567c1ad51be5aa34c01c6651c4d9b5b5ac5d0fd58cf18dd61a47778566b797"
        "a8c67df7b1d60b97b19288d2d877bb2df417ace009dcb0241ca1257d62712b6a4043b4"
        "ff33f690d849da91ea3bf711ed583cb7b7a7da2839ba71309bbf";
    char *comparable_result = malloc(output_len * 2 + 1);
    for (size_t i = 0; i < output_len * 2; i++) {
        sprintf(comparable_result + i, "%c", result[i]);
    }

    FILE *input = fopen("test_input", "rb");

    if (input == NULL) {
        printf("Error opening file\n");
        exit(1);
    }
    system("clear");
    uint8_t *output = malloc(output_len);
    printf("************** BLAKE3 STOUT **************\n");
    test_blake3(has_key, key, derive_key_context, output_len, input, output);

    char *output_hex = malloc(output_len * 2 + 1);
    for (size_t i = 0; i < output_len; i++) {
        sprintf(output_hex + 2 * i, "%02x", output[i]);
    }
    printf("==========================================\n");
    printf("[RESULT]:   %s\n", output_hex);
    printf("[EXPECTED]: %s\n", comparable_result);
    // assert(strcmp(output_hex, comparable_result) == 0);
    // printf("\nSUCCESS\n\n");
    free(output);
    return 0;
}
