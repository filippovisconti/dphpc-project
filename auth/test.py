#! /usr/bin/env python3

import json
import os
import subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
os.chdir(HERE)


EXE = "./blake3"
BUILD_COMMAND = ["make", "blake"]


def test_input(length: int):
    ret = bytearray()
    for i in range(length):
        ret.append(i % 251)
    return ret


def test_run(input_len: int, flags: list[str]) -> str:
    res: bytes = subprocess.run(args=["./blake3"] + flags, input=test_input(
        length=input_len), stdout=subprocess.PIPE, check=True).stdout

    return res.decode().strip()


def main():
    print(" ".join(BUILD_COMMAND))
    subprocess.run(BUILD_COMMAND, check=True)

    with open("test_vectors.json") as f:
        vectors = json.load(f)

    test_key = vectors["key"].encode()
    test_context = vectors["context_string"]
    for case in vectors["cases"]:
        input_len = case["input_len"]
        print("input length:", input_len)

        # regular hashing
        assert test_run(
            input_len, ["--stdin"]) == case["hash"][:64]
        assert test_run(
            input_len, ["--len", "131", "--stdin"]) == case["hash"]
        # keyed hashing
        assert test_run(
            input_len, ["--key", test_key.hex(), "--stdin"]) == case["keyed_hash"][:64]
        assert (test_run(
            input_len, ["--key", test_key.hex(), "--len", "131", "--stdin"]) == case["keyed_hash"]
        )
        # key derivation
        assert (
            test_run(input_len, ["--derive-key", test_context,
                     "--stdin"]) == case["derive_key"][:64]
        )
        assert (
            test_run(input_len, ["--derive-key",
                     test_context, "--len", "131", "--stdin"]) == case["derive_key"]
        )

    print("[SUCCESS:] TESTS PASSED")


if __name__ == "__main__":
    main()
