echo "Generating test data..."
mkdir -p input_data
# base64 -i /dev/urandom | head -c 1000 >input_data/input_1KB.txt
# base64 -i /dev/urandom | head -c 10000 >input_data/input_10KB.txt
# base64 -i /dev/urandom | head -c 100000 >input_data/input_100KB.txt
# base64 -i /dev/urandom | head -c 1000000 >input_data/input_1MB.txt
# base64 -i /dev/urandom | head -c 10000000 >input_data/input_10MB.txt
# base64 -i /dev/urandom | head -c 100000000 >input_data/input_100MB.txt
# base64 -i /dev/urandom | dd bs=1K count=8 > input_data/input_8KB.txt
# base64 -i /dev/urandom | dd bs=1K count=32 > input_data/input_32KB.txt
# base64 -i /dev/urandom | dd bs=1K count=128 > input_data/input_128KB.txt
# base64 -i /dev/urandom | dd bs=1M count=1 > input_data/input_1MB.txt
# base64 -i /dev/urandom | dd bs=1K count=16000 > input_data/input_16MB.txt
# base64 -i /dev/urandom | dd bs=1M count=128 > input_data/input_128MB.txt
# base64 -i /dev/urandom | head -c 1000000000 >input_data/input_1GB.txt
dd if=/dev/urandom of=input_data/input_8KB.txt bs=1K count=8
dd if=/dev/urandom of=input_data/input_32KB.txt bs=1K count=32
dd if=/dev/urandom of=input_data/input_64KB.txt bs=1K count=64
dd if=/dev/urandom of=input_data/input_128KB.txt bs=1K count=128
dd if=/dev/urandom of=input_data/input_1MB.txt bs=1M count=1
dd if=/dev/urandom of=input_data/input_16MB.txt bs=1M count=16
dd if=/dev/urandom of=input_data/input_128MB.txt bs=1M count=128
dd if=/dev/urandom of=input_data/input_1GB.txt bs=1G count=1

echo "Done."
