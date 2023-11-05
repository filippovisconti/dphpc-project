# C port of the BLAKE3 Rust [reference implementation](https://github.com/BLAKE3-team/BLAKE3/blob/master/reference_impl/reference_impl.rs)

[![Actions Status](https://github.com/oconnor663/blake3_reference_impl_c/workflows/tests/badge.svg)](https://github.com/oconnor663/blake3_reference_impl_c/actions)

Run [`test.py`](test.py) to build and test the `blake3` binary. The binary
hashes stdin and prints the result to stdout. You can run it like this:

```shell
$ ./test.py
[build and test output...]
$ echo hello world | ./blake3
dc5a4edb8240b018124052c330270696f96771a63b45250a5c17d3000e823355
$ echo hello world | ./blake3 --len 100
dc5a4edb8240b018124052c330270696f96771a63b45250a5c17d3000e823355675dacfc3ed1a06936ecae2697d6baeaa5e423c0efa51d45b322f3f2ca2ec03d1c5a692d6254d121c20dadf19e0d00e389deb89f2419da878379750df148e9883f482b56
$ echo hello world | ./blake3 --key 0000000000000000000000000000000000000000000000000000000000000000
30f932e14e8cef63f94e658994059fba1a0cf548b01813714c2ce32e2e1c5d3d
```

Note that putting `--key` on the command line as in the example above isn't
something you should do in production, because other processes on your machine
can see your command line arguments. This binary is for demo and testing
purposes only and isn't intended for production use.

This implementation tries to be as simple as possible, and the only performance
optimization here is liberal use of the `inline` keyword. Performance isn't
terrible though, and under `clang -O3` this is a hair faster than Coreutils
`sha512sum` on my laptop.

## How it works

1. **Initialization**:
   - The code defines the initial vector (IV) and message permutation constants (MSG_PERMUTATION). These constants are used in the internal mixing functions to initialize and manipulate the internal state of the hash.
   - `rotate_right` is a helper function to perform circular right shifts on a 32-bit integer.

2. **Mixing Function (G)**:
   - The `g` function is the core mixing function of BLAKE3, which is used to mix either a column or a diagonal of the internal state.
   - It takes four indices (a, b, c, d) and two constants (mx, my) as parameters.
   - The function performs a series of additions, bitwise XORs, and circular right shifts on the internal state to update it. This mixing process ensures that data from different parts of the internal state influences each other.

3. **Round Function**:
   - The `round_function` is a sequence of `g` operations, applied to different parts of the internal state. This function is executed in multiple rounds to thoroughly mix the data within the state.
   - It first mixes columns, and then diagonals of the internal state.

4. **Permutation**:
   - The `permute` function applies a permutation to the 16-word block of data. This permutation rearranges the data according to the constants defined in `MSG_PERMUTATION`.

5. **Compression Function**:
   - The `compress` function is the heart of the BLAKE3 algorithm. It processes a 16-word block and updates the internal state.
   - It combines rounds of the `round_function` with the `permute` step to process the block.

6. **Words from Little-Endian Bytes**:
   - The `words_from_little_endian_bytes` function converts little-endian byte sequences into 32-bit words. BLAKE3 processes data in little-endian order.

7. **Output Structures**:
   - The code defines structures for different types of output, such as chunk output and parent output, to capture the state of the hash at various stages.

8. **Chunk State Management**:
   - Functions like `chunk_state_init`, `chunk_state_update`, and `chunk_state_output` are responsible for managing the state of individual chunks during hashing.

9. **Parent Output and Chaining Value**:
   - The `parent_output` and `parent_cv` functions calculate the output and chaining value for parent nodes in the hash tree. Parent nodes combine the outputs of their child nodes.

10. **Hasher Initialization**:
    - The `blake3_hasher_init` family of functions initializes a BLAKE3 hasher based on different modes of operation (e.g., with or without a key, or key derivation from a context string).

11. **Hashing Process**:
    - The `blake3_hasher_update` function is used to add data to the hash state. It processes data incrementally by compressing it into chunks.

12. **Finalization**:
    - The `blake3_hasher_finalize` function is used to obtain the final hash value by processing any remaining data and aggregating the outputs of chunks and parent nodes.

13. **Main Function (blake3)**:
    - The `blake3` function serves as the entry point for hashing data from an input stream. It initializes the hasher, reads data from the input stream, and computes the hash. The final hash value is then printed in hexadecimal format.

This code provides a complete and efficient implementation of the BLAKE3 hashing algorithm, emphasizing modularity, incremental hashing, and secure handling of keys. It's designed for performance and can process large amounts of data efficiently. The BLAKE3 algorithm's security properties make it suitable for a wide range of applications, including cryptographic hashing and secure key derivation.