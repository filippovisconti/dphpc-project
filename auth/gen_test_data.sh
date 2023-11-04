echo "Generating test data..."
mkdir -p input_data
base64 -i /dev/urandom | head -c 1000 >input_data/input_1KB.txt
base64 -i /dev/urandom | head -c 10000 >input_data/input_10KB.txt
base64 -i /dev/urandom | head -c 100000 >input_data/input_100KB.txt
base64 -i /dev/urandom | head -c 1000000 >input_data/input_1MB.txt
base64 -i /dev/urandom | head -c 10000000 >input_data/input_10MB.txt
base64 -i /dev/urandom | head -c 100000000 >input_data/input_100MB.txt
base64 -i /dev/urandom | head -c 1000000000 >input_data/input_1GB.txt
echo "Done."
