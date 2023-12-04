import os
import random
import string


def generate_random_text_file(filename, size):
    with open(filename, 'w') as file:
        file.write(''.join(random.choice(string.ascii_letters + string.digits)
                   for _ in range(size)))


def main():
    data_sizes = [
        "64_B", "128_B", "156_B", "512_B", "1KB", "2KB", "4KB", "8KB", "32KB",
        "64KB", "129KB", "256KB", "512KB",
        # "1MB", "2MB", "4MB", "16MB", "32MB", "64MB", "128MB", "256MB", "512MB",
        # "1GB"
    ]

    output_dir = "input_data"
    os.makedirs(output_dir, exist_ok=True)

    for size_str in data_sizes:
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
        generate_random_text_file(filename, size)


if __name__ == "__main__":
    main()
