#include "myblake.h"

int blake(char *filename, uint8_t *output, size_t output_len) {
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
      // printf("Read %zu bytes\n", len);
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
      // printf("start chunck %d\n", chunk);
      // START BASE CHUNK PROCESSING
      for (int i = 0; i < num_chunks; i++, chunk++) {
        // printf("Running on chunk %d\n", chunk);

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

          // Each block is parsed in little-endian order into message words m0... m15 and
          // compressed
          uint32_t output_blocks[16];
          // printf("chunck: %d, ", chunk);
          int access_index = (chunk % 4) * BLAKE3_CHUNK_LEN + block * BLAKE3_BLOCK_LEN;
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

          // If a chunk contains only one block,
          // that block sets both CHUNK_START and CHUNK_END.
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
          // printf("block %d input ch val: ", block);
          // for (int i = 0; i < 8; i++) printf("%08x ", input_chaining_value[i]);
          // printf("\n");
          compress(input_chaining_value, output_blocks, counter_t, block_len, flags, out16);
          // copy back the chaining value
          memcpy(chaining_value, out16, sizeof(out16) >> 1);
        }

        // printf("Chunk %d ", chunk);
        // printf("cv: ");
        // for (int i = 0; i < 8; i++) printf("%08x ", chaining_value[i]);
        // printf("\n");
        // chaining_value now contains the whole chunk's chaining value
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
      memcpy(message_words, (void *)(chunk_chaining_values + 2 * i), 32);
      memcpy(message_words + 8, (void *)(chunk_chaining_values + 2 * i + 1), 32);

      uint32_t out16[16];
      compress(input_chaining_value, message_words, counter_t, num_bytes, flags, out16);

      memcpy(buffer_a + i, out16, sizeof(out16) >> 1);
    }
    current_number_of_nodes >>= 1;

    uint8_t a_or_b = 1;
    while (current_number_of_nodes > 1) {
      if (current_number_of_nodes <= 2) flags |= ROOT;

      for (int i = 0; i < (current_number_of_nodes >> 1); i++) {
        memset(message_words, 0, 64);
        if (a_or_b) {
          memcpy(message_words, &buffer_a[2 * i], 32);  // we want the first 32 bytes
          memcpy(message_words + 8, &buffer_a[2 * i + 1], 32);
        } else {
          memcpy(message_words, &buffer_b[2 * i], 32);  // we want the first 32 bytes
          memcpy(message_words + 8, &buffer_b[2 * i + 1], 32);
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

    assert(current_number_of_nodes == 1);
    // Prepare output
    uint32_t *result_buffer      = !a_or_b ? *buffer_b : *buffer_a;
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

    while (running_output_len > 0) {
      uint32_t words[16];

      compress(input_chaining_value, message_words, ++counter_t, num_bytes, flags, words);

      stop = false;

      for (size_t word = 0; word < 16 && !stop; word++) {
        for (int byte = 0; byte < 4; byte++) {
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
    fclose(thread_input);
  }

  return 0;
}
