echo "Generating test data..."
mkdir -p input_data
dd if=/dev/urandom of=input_data/input_8KB.txt bs=1K count=8
dd if=/dev/urandom of=input_data/input_32KB.txt bs=1K count=32
dd if=/dev/urandom of=input_data/input_64KB.txt bs=1K count=64
dd if=/dev/urandom of=input_data/input_128KB.txt bs=1K count=128
dd if=/dev/urandom of=input_data/input_512KB.txt bs=1K count=512
dd if=/dev/urandom of=input_data/input_1MB.txt bs=1M count=1
dd if=/dev/urandom of=input_data/input_2MB.txt bs=1M count=2
dd if=/dev/urandom of=input_data/input_4MB.txt bs=1M count=4
dd if=/dev/urandom of=input_data/input_16MB.txt bs=1M count=16
dd if=/dev/urandom of=input_data/input_32MB.txt bs=1M count=32
dd if=/dev/urandom of=input_data/input_64MB.txt bs=1M count=64
dd if=/dev/urandom of=input_data/input_128MB.txt bs=1M count=128
dd if=/dev/urandom of=input_data/input_256MB.txt bs=1M count=256
dd if=/dev/urandom of=input_data/input_512MB.txt bs=1M count=512
dd if=/dev/urandom of=input_data/input_1GB.txt bs=1M count=1024
dd if=/dev/urandom of=input_data/input_4GB.txt bs=1M count=4096
dd if=/dev/urandom of=input_data/input_8GB.txt bs=1M count=8192
echo "Done."
