
#include "myblake.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "omp.h"
// #define USE_OPENMP
uint32_t IV[8] = {
    0x6A09E667,
    0xBB67AE85,
    0x3C6EF372,
    0xA54FF53A,
    0x510E527F,
    0x9B05688C,
    0x1F83D9AB,
    0x5BE0CD19,
};

size_t MSG_PERMUTATION[16] = {2, 6, 3, 10, 7, 0, 4, 13, 1, 11, 12, 5, 9, 14, 15, 8};

static inline uint32_t rotate_right(uint32_t x, int n) {
  return (x >> n) | (x << (32 - n));
}

static inline void g(
    uint32_t state[16], size_t a, size_t b, size_t c, size_t d, uint32_t mx, uint32_t my) {
  state[a] = state[a] + state[b] + mx;
  state[d] = rotate_right(state[d] ^ state[a], 16);
  state[c] = state[c] + state[d];
  state[b] = rotate_right(state[b] ^ state[c], 12);
  state[a] = state[a] + state[b] + my;
  state[d] = rotate_right(state[d] ^ state[a], 8);
  state[c] = state[c] + state[d];
  state[b] = rotate_right(state[b] ^ state[c], 7);
}

static inline void round_function(uint32_t state[16], uint32_t m[16]) {
  // Mix the columns.
  g(state, 0, 4, 8, 12, m[0], m[1]);
  g(state, 1, 5, 9, 13, m[2], m[3]);
  g(state, 2, 6, 10, 14, m[4], m[5]);
  g(state, 3, 7, 11, 15, m[6], m[7]);
  // Mix the diagonals.
  g(state, 0, 5, 10, 15, m[8], m[9]);
  g(state, 1, 6, 11, 12, m[10], m[11]);
  g(state, 2, 7, 8, 13, m[12], m[13]);
  g(state, 3, 4, 9, 14, m[14], m[15]);
}

static inline void permute(uint32_t m[16]) {
  uint32_t permuted[16];
  for (size_t i = 0; i < 16; i++) { permuted[i] = m[MSG_PERMUTATION[i]]; }
  memcpy(m, permuted, sizeof(permuted));
}

static inline void compress(const uint32_t chaining_value[8], const uint32_t block_words[16],
    uint64_t counter, uint32_t block_len, uint32_t flags, uint32_t out[16]) {
  (void)block_len;
  uint32_t state[16] = {
      chaining_value[0],
      chaining_value[1],
      chaining_value[2],
      chaining_value[3],
      chaining_value[4],
      chaining_value[5],
      chaining_value[6],
      chaining_value[7],
      IV[0],
      IV[1],
      IV[2],
      IV[3],
      (uint32_t)counter,
      (uint32_t)(counter >> 32),
      BLAKE3_BLOCK_LEN,
      flags,
  };
  uint32_t block[16];
  memcpy(block, block_words, sizeof(block));

  round_function(state, block);  // round 1
  permute(block);
  round_function(state, block);  // round 2
  permute(block);
  round_function(state, block);  // round 3
  permute(block);
  round_function(state, block);  // round 4
  permute(block);
  round_function(state, block);  // round 5
  permute(block);
  round_function(state, block);  // round 6
  permute(block);
  round_function(state, block);  // round 7

  for (size_t i = 0; i < 8; i++) {
    state[i] ^= state[i + 8];
    state[i + 8] ^= chaining_value[i];
  }

  memcpy(out, state, sizeof(state));
}

static inline void words_from_little_endian_bytes(
    const void *bytes, size_t bytes_len, uint32_t *out) {
  assert(bytes_len % 4 == 0);
  const uint8_t *u8_ptr = (const uint8_t *)bytes;
  for (size_t i = 0; i < (bytes_len / 4); i++) {
    out[i] = ((uint32_t)(*u8_ptr++));
    out[i] += ((uint32_t)(*u8_ptr++)) << 8;
    out[i] += ((uint32_t)(*u8_ptr++)) << 16;
    out[i] += ((uint32_t)(*u8_ptr++)) << 24;
  }
}

void myblake(char *filename, uint8_t *output, size_t output_len) {
  FILE *input = fopen(filename, "rb");

  if (input == NULL) {
    printf("Error opening file\n");
    exit(1);
  }

  fseek(input, 0L, SEEK_END);
  int num_chunks = ftell(input);
  // printf("Number of chunks: %d\n", num_chunks);
  num_chunks >>= CHUNK_SIZE_LOG;  // / 1024
  // printf("Number of chunks: %d\n", num_chunks);
  bool chunks_pow_of_two = (num_chunks & (num_chunks - 1)) == 0;

  fclose(input);
  if (!chunks_pow_of_two) {
    printf("Number of chunks is not a power of two\n");
    exit(1);
  }
#ifdef USE_OPENMP
  int num_threads = omp_get_max_threads();
  printf("[OMP] Number of available threads: %d\n", num_threads);
  num_threads = 1 << (int)log2(num_threads);  // get largest power of 2 smaller than num_threads
  omp_set_dynamic(0);
  if (num_chunks > (num_threads << 2))
    omp_set_num_threads(num_threads);  // each thread handles at least 4 chunks
  else {
    num_threads = 2;
    omp_set_num_threads(num_threads);  // not worth parallelizing, use only one thread
  }
  printf("[OMP] Number of usable threads: %d\n", omp_get_max_threads());
  assert(omp_get_max_threads() == num_threads);
  printf("[OMP] Running with num_threads: %d\n", num_threads);
#else
  int      num_threads = 2;
#endif
  size_t level     = (int)log2(num_threads);  // tree depth in which we have one node per thread
  size_t max_depth = (int)ceil(log2(num_chunks));  // maximum depth of the tree
  size_t sub_tree_depth = max_depth - level;       // depth of each thread's subtree
  size_t num_leaves     = 1 << sub_tree_depth;     // 2^sub_tree_depth

#ifdef USE_OPENMP
  uint32_t parent_chaining_values[num_threads][8];
#pragma omp parallel
#else
  uint32_t parent_chaining_values[2][8];
  for (int thread_num = 0; thread_num < 2; thread_num++)
#endif
  {
#ifdef USE_OPENMP
    int thread_num = omp_get_thread_num();
    assert(thread_num <= num_threads);
#endif
    int stride    = (thread_num * num_leaves) << CHUNK_SIZE_LOG;
    int read_size = num_leaves << CHUNK_SIZE_LOG;

    FILE *thread_input = fopen(filename, "rb");
    fseek(thread_input, stride, SEEK_SET);

    // uint32_t chunk_chaining_values[num_leaves][8];
    uint32_t(*chunk_chaining_values)[8] = calloc(num_leaves, sizeof(uint32_t[8]));
    if (chunk_chaining_values == NULL) {
      fprintf(stderr, "Memory allocation failed\n");
      exit(EXIT_FAILURE);
    }

    int chunk_counter = 0;
    // memset(chunk_chaining_values, 0, sizeof(chunk_chaining_values));
    int chunk = 0;
    while (!feof(thread_input) && read_size > 0) {
      char  *read_buffer = malloc(4 * CHUNK_SIZE);
      size_t len         = fread(read_buffer, 1, 4 * CHUNK_SIZE, thread_input);
      if ((len == 0) || (len > (num_leaves << CHUNK_SIZE_LOG))) { break; }  // EOF

      read_size -= len;

      int num_read_chunks = CEIL_DIV(len, CHUNK_SIZE);
      if (num_read_chunks > 4) {
        printf("failed\n");
        exit(1);
      }
      // START BASE CHUNK PROCESSING
      // printf("thread_num: %d | chunk: %d\n", thread_num, chunk);
      for (int i = 0; i < num_read_chunks; i++, chunk++) {
        int  num_blocks = 16;
        bool last_chunk = false;
        int  remainder  = (len - CHUNK_SIZE * (num_read_chunks - 1));  // 0 <= remainder <= 1024
        if (chunk == num_read_chunks - 1) {
          num_blocks = CEIL_DIV(remainder, BLAKE3_BLOCK_LEN);
          last_chunk = true;
        }

        uint32_t chaining_value[8];
        uint64_t counter_t = chunk + num_leaves * thread_num;
        for (int block = 0; block < num_blocks; block++) {

          int block_len = BLAKE3_BLOCK_LEN;
          if (block == num_blocks - 1 && last_chunk) {
            if ((remainder & (BLAKE3_BLOCK_LEN - 1)) == 0) {
              block_len = remainder % BLAKE3_BLOCK_LEN;
              if (block_len < BLAKE3_BLOCK_LEN) {
                memset(&read_buffer[block * BLAKE3_BLOCK_LEN + block_len], 0,
                    BLAKE3_BLOCK_LEN - block_len);
              }
            }
          }

          uint32_t output_blocks[16];
          int      access_index = (chunk % 4) * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
          words_from_little_endian_bytes(
              &read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

          // If a chunk is the root of its tree, the last block of that chunk also
          // TODO: sets the ROOT flag.
          uint32_t flags = 0;
          if (block == 0) flags |= CHUNK_START;
          if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;

          if (num_blocks == 1) assert(flags == (CHUNK_START | CHUNK_END));

          const uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);
          uint32_t        out16[16];
          compress(input_chaining_value, output_blocks, counter_t, block_len, flags, out16);
          memcpy(chaining_value, out16, sizeof(out16) >> 1);
        }

        // save last block's cv for parent processing
        // memcpy(chunk_chaining_values[chunk_counter++], chaining_value, sizeof(chaining_value));
        if (chunk_counter >= (int)num_leaves) {
          printf("chunk_counter: %d\n", chunk_counter);
          printf("num_leaves: %ld\n", num_leaves);
        }
        assert(chunk_counter < (int)num_leaves);
        for (int j = 0; j < 8; j++) { chunk_chaining_values[chunk_counter][j] = chaining_value[j]; }
        chunk_counter++;
      }
      // END BASE CHUNK PROCESSING
    }
    // printf("chunk_chaining_values:\n");
    // for (size_t i = 0; i < num_leaves; i++) {
    //   for (int j = 0; j < 8; j++) { printf("%08x ", chunk_chaining_values[i][j]); }
    //   printf("\n");
    // }
    // START PARENT PROCESSING
    int       counter_t = 0;       // always 0 for parent nodes
    const int num_bytes = 64;      // always 64
    uint32_t  flags     = PARENT;  // set parent flag

    const uint32_t *input_chaining_value    = IV;  // input ch val is the keywords
    int             current_number_of_nodes = num_leaves;
    uint32_t        message_words[16];  // first 8 is left child, second 8 is the right one;

    uint32_t(*buffer_a)[8] = (uint32_t(*)[8])calloc(num_leaves / 2, sizeof(uint32_t[8]));

    if (buffer_a == NULL) {
      fprintf(stderr, "Memory allocation failed for buffer_a\n");
      exit(EXIT_FAILURE);
    }
    uint32_t(*buffer_b)[8] = (uint32_t(*)[8])calloc(num_leaves / 4, sizeof(uint32_t[8]));

    if (buffer_b == NULL) {
      fprintf(stderr, "Memory allocation failed for buffer_b\n");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
      // printf("thread_num: %d | i: %d\n", thread_num, i);
      // we want the first 32 bytes
      memcpy(message_words + 0, (&chunk_chaining_values[2 * i + 0]), 32);
      memcpy(message_words + 8, (&chunk_chaining_values[2 * i + 1]), 32);

      uint32_t out16[16];
      compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);

      memcpy(buffer_a + i, out16, sizeof(out16) >> 1);
    }
    current_number_of_nodes >>= 1;
    free(chunk_chaining_values);
    uint8_t a_or_b = 1;
    while (current_number_of_nodes > 1) {
      if (a_or_b) {
        memset(buffer_b, 0, num_leaves / 4 * sizeof(uint32_t[8]));
      } else {
        memset(buffer_a, 0, num_leaves / 2 * sizeof(uint32_t[8]));
      }
      for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
        memset(message_words, 0, 64);
        if (a_or_b) {
          memcpy(message_words + 0, &buffer_a[2 * i][0], 32);
          memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
        } else {
          memcpy(message_words + 0, &buffer_b[2 * i][0], 32);
          memcpy(message_words + 8, &buffer_b[2 * i + 1][0], 32);
        }

        uint32_t out16[16];
        compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);

        if (a_or_b) {
          memcpy(&buffer_b[i], out16, sizeof(out16) >> 1);
        } else {
          memcpy(&buffer_a[i], out16, sizeof(out16) >> 1);
        }
      }
      current_number_of_nodes >>= 1;
      a_or_b = !a_or_b;
    }
    if (current_number_of_nodes != 1)
      printf("current_number_of_nodes: %d\n", current_number_of_nodes);
    assert(current_number_of_nodes == 1);
    assert(thread_num < num_threads);
    if (a_or_b) {
      // memcpy(parent_chaining_values[thread_num], buffer_a[0], 8 * 32);
      for (int j = 0; j < 8; j++) { parent_chaining_values[thread_num][j] = buffer_a[0][j]; }
    } else {
      // memcpy(parent_chaining_values[thread_num], buffer_b[0], 8 * 32);
      for (int j = 0; j < 8; j++) { parent_chaining_values[thread_num][j] = buffer_b[0][j]; }
    }
    free(buffer_a);
    free(buffer_b);
    fclose(thread_input);
  }
  // END PARALLEL REGION
  printf("END PARALLEL REGION\n");
  int       counter_t = 0;       // always 0 for parent nodes
  const int num_bytes = 64;      // always 64
  uint32_t  flags     = PARENT;  // set parent flag

  const uint32_t *input_chaining_value = IV;  // input ch val is the keywords
  uint32_t        message_words[16];          // first 8 is left child, second 8 is the right one;
  int             current_number_of_nodes = num_threads;

  uint32_t buffer_a[num_leaves / 2][8];
  uint32_t buffer_b[num_leaves / 4][8];
  memset(buffer_a, 0, sizeof(buffer_a));
  memset(buffer_b, 0, sizeof(buffer_b));

  for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
    // we want the first 32 bytes
    memcpy(message_words + 0, (&parent_chaining_values[2 * i + 0]), 32);
    memcpy(message_words + 8, (&parent_chaining_values[2 * i + 1]), 32);

    uint32_t out16[16];
    if (current_number_of_nodes <= 2) flags |= ROOT;
    compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);
    memcpy(buffer_a + i, out16, sizeof(out16) >> 1);
  }
  current_number_of_nodes >>= 1;

  uint8_t a_or_b = 1;
  while (current_number_of_nodes > 1) {
    if (current_number_of_nodes <= 2) flags |= ROOT;
    if (a_or_b) {
      memset(buffer_b, 0, sizeof(buffer_b));
    } else {
      memset(buffer_a, 0, sizeof(buffer_a));
    }
    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
      memset(message_words, 0, 64);
      if (a_or_b) {
        memcpy(message_words + 0, &buffer_a[2 * i][0], 32);
        memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
      } else {
        memcpy(message_words + 0, &buffer_b[2 * i][0], 32);
        memcpy(message_words + 8, &buffer_b[2 * i + 1][0], 32);
      }

      uint32_t out16[16];
      compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);
      // print all input of compress function

      if (a_or_b) {
        memcpy(&buffer_b[i], out16, sizeof(out16) >> 1);
      } else {
        memcpy(&buffer_a[i], out16, sizeof(out16) >> 1);
      }
    }
    current_number_of_nodes >>= 1;
    a_or_b = !a_or_b;
  }
  if (current_number_of_nodes != 1)
    printf("current_number_of_nodes: %d\n", current_number_of_nodes);
  assert(current_number_of_nodes == 1);
  // Prepare output
  uint8_t *running_output     = output;
  size_t   running_output_len = output_len;
  printf("output_len: %ld\n", output_len);
  memset(output, 0, output_len);

  bool stop = false;
  counter_t--;
  while (running_output_len > 0) {
    uint32_t words[16];

    compress(input_chaining_value, message_words, ++counter_t, num_bytes, flags, words);

    stop = false;

    for (size_t word = 0; word < 16 && !stop; word++) {
      for (int byte = 0; byte < 4; byte++) {
        if (output_len == 0) {
          printf("FINISHED\n");
          stop = true;
          break;
        };

        *running_output = (uint8_t)(words[word] >> (8 * byte));
        running_output++;
        running_output_len--;
      }
    }
  }
}

#if defined MYBLAKE_MAIN
int         main(void) {
  char    filename[] = "test_input";
  size_t  output_len = 128;
  uint8_t output[output_len];

  myblake(filename, output, output_len);

  printf("\n\nOutput: ");
  for (size_t i = 0; i < output_len; i++) {
    if (i % 32 == 0) printf("\n");
    printf("%02x", output[i]);
  }
  printf("\n");
  return 0;
}
#endif
