
#include "myblake.h"

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
  size_t num_chunks        = ftell(input) >> CHUNK_SIZE_LOG;  // / 1024
  bool   chunks_pow_of_two = (num_chunks & (num_chunks - 1)) == 0;

  if (!chunks_pow_of_two) {
    printf("Number of chunks is not a power of two\n");
    exit(1);
  }

  fclose(input);

  printf("Number of chunks: %zu\n", num_chunks);

  size_t   num_threads = 1;                       // 8;
  size_t   level       = (int)log2(num_threads);  // tree depth in which we have one node per thread
  size_t   max_depth   = (int)ceil(log2(num_chunks));  // maximum depth of the tree
  uint64_t sub_tree_depth = max_depth - level;         // depth of each thread's subtree
  uint64_t num_leaves     = 1 << sub_tree_depth;       // 2^sub_tree_depth
  size_t   t              = 0;

  {
    int stride = t * num_leaves << CHUNK_SIZE_LOG;
    printf("stride %d\n", stride);
    int read_size = num_leaves << CHUNK_SIZE_LOG;
    printf("read_size %d\n", read_size);

    FILE *thread_input = fopen(filename, "rb");
    fseek(thread_input, stride, SEEK_SET);
    printf("Thread %zu: %d\n\n", t, stride);

    uint32_t chunk_chaining_values[num_chunks][8];
    int      chunk = 0;
    while (!feof(thread_input) && read_size > 0) {
      char  *read_buffer = malloc(4 * CHUNK_SIZE);
      size_t len         = fread(read_buffer, 1, 4 * CHUNK_SIZE, thread_input);
      if ((len == 0) || (len >= (num_leaves << CHUNK_SIZE_LOG))) {
        printf("finished\n");
        break;
      }  // EOF

      read_size -= len;

      int num_chunks = CEIL_DIV(len, CHUNK_SIZE);
      if (num_chunks > 4) {
        printf("failed\n");
        exit(1);
      }
      // START BASE CHUNK PROCESSING
      for (int i = 0; i < num_chunks; i++, chunk++) {

        int  num_blocks = 16;
        bool last_chunk = false;
        int  remainder  = (len - CHUNK_SIZE * (num_chunks - 1));  // 0 <= remainder <= 1024
        if (chunk == num_chunks - 1) {
          num_blocks = CEIL_DIV(remainder, BLAKE3_BLOCK_LEN);
          last_chunk = true;
        }

        uint32_t chaining_value[8];
        for (int block = 0; block < num_blocks; block++) {
          int      block_len = BLAKE3_BLOCK_LEN;
          uint32_t flags     = 0;

          // if we're in the last block of the last chunk
          if (block == num_blocks - 1 && last_chunk) {
            // printf("last block of last chunk\n");

            // if (len % BLAKE3_BLOCK_LEN != 0)
            // if remainder is not a multiple of 64, last block is not full
            if ((remainder & (BLAKE3_BLOCK_LEN - 1)) == 0) {
              // printf("last block is full\n");

              // calculate how many bytes are present
              block_len = remainder % BLAKE3_BLOCK_LEN;

              // if last block has less than 64 bytes
              if (block_len < BLAKE3_BLOCK_LEN) {

                // pad with zeros
                memset(&read_buffer[block * BLAKE3_BLOCK_LEN + block_len], 0,
                    BLAKE3_BLOCK_LEN - block_len);
              }
            }
          }

          uint32_t output_blocks[16];
          // printf("chunck: %d, ", chunk);
          int access_index = (chunk % 4) * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
          words_from_little_endian_bytes(
              &read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

          if (block == 0) flags |= CHUNK_START;
          if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;

          int counter_t = chunk;  // TODO: useless variable, currently

          if (num_blocks == 1) assert(flags == (CHUNK_START | CHUNK_END));

          // If a chunk is the root of its tree, the last block of that chunk also
          // TODO: sets the ROOT flag.

          const uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);

          uint32_t out16[16];
          compress(input_chaining_value, output_blocks, counter_t, block_len, flags, out16);
          memcpy(chaining_value, out16, sizeof(out16) >> 1);
        }

        // save for parent processing
        memcpy(chunk_chaining_values[chunk], chaining_value, sizeof(chaining_value));
      }
      // END BASE CHUNK PROCESSING
    }
    printf("Finished reading\n\n");

    // START PARENT PROCESSING
    int       counter_t = 0;       // always 0 for parent nodes
    const int num_bytes = 64;      // always 64
    uint32_t  flags     = PARENT;  // set parent flag

    const uint32_t *input_chaining_value = IV;  // input ch val is the keywords
    uint32_t        message_words[16];          // first 8 is left child, second 8 is the right one;

    int current_number_of_nodes = num_chunks;

    uint32_t buffer_a[num_chunks / 2][8];
    uint32_t buffer_b[num_chunks / 4][8];
    memset(buffer_a, 0, sizeof(buffer_a));
    memset(buffer_b, 0, sizeof(buffer_b));

    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
      // we want the first 32 bytes
      memcpy(message_words + 0, (&chunk_chaining_values[2 * i + 0]), 32);
      memcpy(message_words + 8, (&chunk_chaining_values[2 * i + 1]), 32);

      uint32_t out16[16];
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
          memcpy(message_words, &buffer_a[2 * i][0], 32);
          memcpy(message_words + 8, &buffer_a[2 * i + 1][0], 32);
        } else {
          memcpy(message_words, &buffer_b[2 * i][0], 32);
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
    // END PARRENT PROCESSING
    fclose(thread_input);
  }
}
#if defined MYBLAKE_MAIN
int         main(void) {
  char    filename[] = "test_input";
  size_t  output_len = 128;
  uint8_t output[output_len];

  myblake(filename, output, output_len);

  printf("Output: ");
  for (size_t i = 0; i < output_len; i++) {
    if (i % 32 == 0) printf("\n");
    printf("%02x", output[i]);
  }
  printf("\n");
  return 0;
}
#endif
