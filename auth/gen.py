import os
import random
import string


def generate_random_text_file(filename, size):
    with open(filename, 'w') as file:
        file.write(''.join(random.choice(string.ascii_letters + string.digits)
                   for _ in range(size)))


def main():
    with open("input_data_sizes.txt", 'r') as file:
        data_sizes = file.read().splitlines()

    output_dir = "input_data"
    os.makedirs(output_dir, exist_ok=True)

    for size_str in data_sizes:
        if size_str.startswith("#"):
            print(f"Skipping {size_str}...commented out")
            continue

        print(f"Generating {size_str} file...")
        size = int(size_str[:-2])  # Extract the numeric part
        unit = size_str[-2:]  # Extract the unit (KB, MB, GB)

        if unit == "KB":
            size *= 1024
        elif unit == "MB":
            size *= 1024 * 1024
        elif unit == "GB":
            size *= 1024 * 1024 * 1024

        filename = os.path.join(output_dir, f'input_{size_str}.txt')
        if os.path.exists(filename):
            print(f"Skipping {filename}... already exists.")
            continue
        generate_random_text_file(filename, size)


if __name__ == "__main__":
    main()
