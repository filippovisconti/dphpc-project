#ifndef _MYBLAKE3
#define _MYBLAKE3

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "omp.h"

#define CHUNK_START         1 << 0
#define CHUNK_END           1 << 1
#define PARENT              1 << 2
#define ROOT                1 << 3
#define KEYED_HASH          1 << 4
#define DERIVE_KEY_CONTEXT  1 << 5
#define DERIVE_KEY_MATERIAL 1 << 6
#define BLAKE3_OUT_LEN      32
#define BLAKE3_KEY_LEN      32
#define BLAKE3_BLOCK_LEN    64
#define BLAKE3_CHUNK_LEN    1024
#define SMALL_BLOCK_SIZE    1024

#define CHUNK_SIZE          1024
#define CHUNK_SIZE_LOG      10

#define CEIL_DIV(x, y)      ((int)ceil((double)(x) / (y)))

void myblake(char *filename, uint8_t *output, size_t output_len);

#endif
