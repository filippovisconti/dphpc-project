#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "myblake.h"
#include "reference_impl.h"

int main(void) {
  uint8_t     key[BLAKE3_KEY_LEN];
  bool        has_key            = false;
  const char *derive_key_context = NULL;
  size_t      output_len         = BLAKE3_OUT_LEN * 4;

  FILE *input = fopen("test_input", "rb");

  if (input == NULL) {
    printf("Error opening file\n");
    exit(1);
  }
  // system("clear");
  uint8_t *output_ref = malloc(output_len);
  uint8_t *output_my  = malloc(output_len);
  printf("************** REFERENCE BLAKE3 STOUT **************\n");
  test_blake3(has_key, key, derive_key_context, output_len, input, output_ref);
  printf("************** MY BLAKE3 STOUT *********************\n");
  myblake("test_input", output_my, output_len);
  printf("****************************************************\n");


  for (size_t i = 0; i < output_len; i++) {
    if (output_ref[i] != output_my[i]) {
      printf("ERROR: %zu | ", i);
      printf("REF: %02x | ", output_ref[i]);
      printf("GOT: %02x\n", output_my[i]);
    }
  }

  char *output_hex_ref = malloc(output_len * 2 + 1);
  char *output_hex     = malloc(output_len * 2 + 1);
  for (size_t i = 0; i < output_len; i++) {
    sprintf(output_hex_ref + 2 * i, "%02x", output_ref[i]);
    sprintf(output_hex + 2 * i, "%02x", output_my[i]);
  }

  //=========================================
  printf("[REFERENCE]:\n");
  PRINT_WITH_NEWLINE(output_hex_ref, 64);
  printf("\n");
  printf("[CALCULATED]:\n");
  PRINT_WITH_NEWLINE(output_hex, 64);
  printf("\n");
  assert(strcmp(output_hex_ref, output_hex) == 0);
  // bool cond = strcmp(output_hex, comparable_result) == 0;
  // if (!cond) exit(1);
  printf("\nSUCCESS\n\n");
  free(output_ref);
  return 0;
}
