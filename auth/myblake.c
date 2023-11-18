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

// inline long double log2(const long double x) {
//     return log(x) * M_LOG2E;
// }

/**
 * Rotates the bits of a 32-bit unsigned integer to the right by a given number
 * of positions.
 *
 * @param x The 32-bit unsigned integer to rotate.
 * @param n The number of positions to rotate the bits to the right.
 * @return The result of rotating the bits of x to the right by n positions.
 */
static inline uint32_t rotate_right(uint32_t x, int n) {
  return (x >> n) | (x << (32 - n));
}

// The mixing function, G, which mixes either a column or a diagonal.
/**
 * Performs the G function of the BLAKE hash algorithm on the given state and
 * message words.
 *
 * @param state The current state of the hash algorithm.
 * @param a The index of the first state word to use in the function.
 * @param b The index of the second state word to use in the function.
 * @param c The index of the third state word to use in the function.
 * @param d The index of the fourth state word to use in the function.
 * @param mx The first message word to use in the function.
 * @param my The second message word to use in the function.
 */
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

/**
 * Permutes the given message block using the BLAKE message permutation.
 *
 * @param m The message block to permute.
 */
static inline void permute(uint32_t m[16]) {
  uint32_t permuted[16];
  for (size_t i = 0; i < 16; i++) { permuted[i] = m[MSG_PERMUTATION[i]]; }
  memcpy(m, permuted, sizeof(permuted));
}

/**
 * Compresses a block of data using the BLAKE hash function.
 *
 * @param chaining_value The current chaining value.
 * @param block_words The block of data to compress.
 * @param counter The current counter value.
 * @param BLAKE3_BLOCK_LEN The length of the block in bytes.
 * @param flags The current flag value.
 * @param out The resulting hash value.
 */
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

/**
 * Converts an array of little-endian bytes to an array of 32-bit words.
 *
 * @param bytes Pointer to the array of bytes to be converted.
 * @param bytes_len Length of the byte array. Must be a multiple of 4.
 * @param out Pointer to the output array of 32-bit words.
 */
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

int blake(char *filename) {
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
  size_t  output_len = 128;
  uint8_t output[output_len];
  printf("Number of chunks: %zu\n", num_chunks);

  size_t   num_threads = 1;                       // 8;
  size_t   level       = (int)log2(num_threads);  // tree depth in which we have one node per thread
  size_t   max_depth   = (int)ceil(log2(num_chunks));  // maximum depth of the tree
  uint64_t sub_tree_depth = max_depth - level;         // depth of each thread's subtree
  uint64_t num_leaves     = 1 << sub_tree_depth;       // 2^sub_tree_depth

  for (size_t t = 0; t < num_threads; t++) {
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
      // reading 4 chunks of 1024 bytes each at a time
      char  *read_buffer = malloc(4 * CHUNK_SIZE);
      size_t len         = fread(read_buffer, 1, 4 * CHUNK_SIZE, thread_input);
      printf("Read %zu bytes\n", len);
      if ((len == 0) || (len >= (num_leaves << CHUNK_SIZE_LOG))) {
        printf("finished\n");
        break;
      }  // EOF

      read_size -= len;

      // (int)ceil((double)len / CHUNK_SIZE);  // 1 <= num_chunks <= 4
      int num_chunks = CEIL_DIV(len, CHUNK_SIZE);
      if (num_chunks > 4) {
        printf("failed\n");
        exit(1);
      }
      printf("start chunck %d\n", chunk);
      // START BASE CHUNK PROCESSING
      for (int i = 0; i < num_chunks; i++, chunk++) {
        printf("Running on chunk %d\n", chunk);

        // Each chunk of up to 1024 bytes is split into blocks of up to 64 bytes.
        int  num_blocks = 16;
        bool last_chunk = false;
        int  remainder  = (len - CHUNK_SIZE * (num_chunks - 1));  // 0 <= remainder <= 1024
        if (chunk == num_chunks - 1) {
          //(int)ceil((double)remainder / BLAKE3_BLOCK_LEN);
          num_blocks = CEIL_DIV(remainder, BLAKE3_BLOCK_LEN);
          last_chunk = true;
        }

        uint32_t chaining_value[8];
        for (int block = 0; block < num_blocks; block++) {
          // The block length b is the number of input bytes in each block,
          // i.e., 64 for all blocks except the last block of the last chunk,
          // which may be short.
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
          int access_index = chunk * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;

          // Each block is parsed in little-endian order into message words m0... m15 and
          // compressed
          uint32_t output_blocks[16];
          words_from_little_endian_bytes(
              &read_buffer[access_index], BLAKE3_BLOCK_LEN, output_blocks);

          if (block == 0) flags |= CHUNK_START;
          // The first block of each chunk sets the CHUNK_START flag
          // the last block of each chunk sets the CHUNK_END flag.
          if ((block % num_blocks) == (num_blocks - 1)) flags |= CHUNK_END;

          // The counter t for each block is the chunk index i.e.:
          // 0 for all blocks in the first chunk,
          // 1 for all blocks in the second chunk, and so on.
          int counter_t = chunk;  // TODO: useless variable, currently

          // If a chunk contains only one block, that block sets both CHUNK_START and
          // CHUNK_END.
          if (num_blocks == 1) assert(flags == (CHUNK_START | CHUNK_END));

          // If a chunk is the root of its tree, the last block of that chunk also
          // TODO: sets the ROOT flag.

          // The input chaining value h0-h7 for the first block of each chunk is composed
          // of the key words k0-k7, or in the base case it's the IV vector. The input
          // chaining value for subsequent blocks is the output of the compression
          // function for the previous block
          const uint32_t *input_chaining_value = (block == 0 ? IV : chaining_value);

          // The output of the truncated compression function for the last block in a
          // chunk is the chaining value of that chunk.
          uint32_t out16[16];
          // printf("flags: %08x\n", flags);
          printf("block %d input ch val: ", block);
          for (int i = 0; i < 8; i++) printf("%08x ", input_chaining_value[i]);
          printf("\n");
          compress(input_chaining_value, output_blocks, counter_t, block_len, flags, out16);
          // copy back the chaining value
          memcpy(chaining_value, out16, sizeof(out16) >> 1);
        }

        printf("Chunk %d ", chunk);
        printf("cv: ");
        for (int i = 0; i < 8; i++) printf("%08x ", chaining_value[i]);
        printf("\n");
        // chaining_value now contains the whole chunk's chaining value
        // save for parent processing
        memcpy(chunk_chaining_values[chunk], chaining_value, sizeof(chaining_value));
      }
      // END BASE CHUNK PROCESSING
    }
    printf("Finished reading\n\n");

    // START PARENT PROCESSING
    printf("Parent processing\n");
    int      counter_t = 0;                     // always 0 for parent nodes
    int      num_bytes = 64;                    // always 64
    uint32_t flags     = PARENT;                // set parent flag
                                                //
    const uint32_t *input_chaining_value = IV;  // input ch val is the keywords
    uint32_t        message_words[16];          // first 8 is left child, second 8 is the right one;

    int current_number_of_nodes = num_chunks;

    uint32_t *buffer_a = malloc((current_number_of_nodes >> 1) << 5);
    memset(buffer_a, 0, (current_number_of_nodes >> 1) << 5);
    uint32_t *buffer_b = malloc((current_number_of_nodes >> 2) << 5);  // (num/2) * 4*8
    memset(buffer_b, 0, (current_number_of_nodes >> 1) << 5);

    printf("current_number_of_nodes: %d\n", current_number_of_nodes);
    uint8_t a_or_b = 1;
    printf("input_chaining_value: ");
    for (int i = 0; i < 8; i++) printf("%08x ", input_chaining_value[i]);
    printf("\n");

    for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
      memcpy(message_words, (void *)(chunk_chaining_values + 2 * i), 32);
      // we want the first 32 bytes
      memcpy(message_words + 8, (void *)(chunk_chaining_values + 2 * i + 1), 32);

      printf("\nmessage_words:\n");
      for (int i = 0; i < 8; i++) printf("%08x ", message_words[i]);
      printf("\n");
      for (int i = 8; i < 16; i++) printf("%08x ", message_words[i]);
      printf("\n");

      uint32_t out16[16];
      compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);

      printf("Current cv:\n");
      for (int i = 0; i < 8; i++) printf("%08x ", out16[i]);
      printf("\n");
      // TODO +i is wrong
      memcpy(buffer_a + i, out16, sizeof(out16) >> 1);
    }
    current_number_of_nodes >>= 1;

    while (current_number_of_nodes > 1) {
      printf("using buffer %d\n", a_or_b);
      printf("current_number_of_nodes: %d\n", current_number_of_nodes);

      uint32_t *buffer      = a_or_b ? buffer_a : buffer_b;
      uint32_t *buffer_next = a_or_b ? buffer_b : buffer_a;
      buffer_next           = realloc(buffer_next, (current_number_of_nodes >> 1) << 5);

      if (a_or_b) buffer_b = buffer_next;
      else buffer_a = buffer_next;

      // if (current_number_of_nodes <= 2) flags |= ROOT;

      for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
        memset(message_words, 0, 64);
        memcpy(message_words, buffer + 2 * i, 32);  // we want the first 32 bytes
        memcpy(message_words + 8, buffer + 2 * i + 1, 32);

        uint32_t out16[16];
        compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);

        memcpy(buffer_next + i, out16, sizeof(out16) >> 1);
      }
      current_number_of_nodes >>= 1;
      a_or_b = !a_or_b;
    }

    assert(current_number_of_nodes == 1);
    printf("now we have only one node\n");
    // Prepare output
    uint32_t *result_buffer      = a_or_b ? buffer_b : buffer_a;
    uint8_t  *running_output     = output;
    size_t    running_output_len = output_len;
    printf("output_len: %ld\n", output_len);
    memset(output, 0, output_len);

    bool stop = false;
    for (size_t word = 0; word < 16 && !stop; word++) {
      for (int byte = 0; byte < 4; byte++) {
        // output[word * 4 + byte] = (result_buffer[word] >> (8 * byte)) & 0xFF;
        if (running_output_len == 0) {
          stop = true;
          printf("FINISHED\n");
          break;
        };

        *running_output = (uint8_t)(result_buffer[word] >> (8 * byte)) & 0xFF;
        running_output++;
        running_output_len--;
      }
    }

    printf("Finished first output round\nOutput: ");

    for (size_t i = 0; i < output_len; i++) printf("%02x", output[i]);
    printf("\n");

    while (running_output_len > 0) {
      uint32_t words[16];
      compress(input_chaining_value, message_words, ++counter_t, num_bytes, flags, words);
      stop = false;

      for (size_t word = 0; word < 16 && !stop; word++) {
        for (int byte = 0; byte < 4; byte++) {
          // output[word * 4 + byte] = (words[word] >> (8 * byte)) & 0xFF;
          if (output_len == 0) {
            stop = true;
            break;
          };

          *running_output = (uint8_t)(words[word] >> (8 * byte));
          running_output++;
          running_output_len--;
        }
      }
    }
    printf("Output: ");
    for (size_t i = 0; i < output_len; i++) printf("%02x", output[i]);
    printf("\n");

    // END PARRENT PROCESSING
    free(buffer_a);
    free(buffer_b);
    fclose(thread_input);
  }

  return 0;
}

int main(void) {
  char filename[] = "test_input";
  blake(filename);

  return 0;
}
