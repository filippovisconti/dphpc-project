#!/bin/bash
set -e
# set -x

echo "Generating test data..."
input="./input_sizes.txt"

mkdir -p input_data
while read -r line
do
  #skip empty lines
  [[ -z "$line" ]] && continue
  #skip line starting with #
  [[ $line = \#* ]] && echo "Skipping $line" && continue

    # skip if file already exists
  [[ -f "./input_data/input_$line.txt" ]] && echo "File exists $line" && continue

  fsize=$(echo "$line" | grep -o '[0-9]\+')
  unit=$(echo "$line" | grep -o '[a-zA-Z]\+')
  base="1"
  if [ "$unit" = "KB" ]; then
    base="1K"
  elif [ "$unit" = "MB" ]; then
    base="1M"
  elif [ "$unit" = "GB" ]; then
    base="1G"
  fi

  dd if=/dev/urandom of=./input_data/input_$line.txt bs=$base count=$fsize

  echo "$line"
done < "$input"

# for line in file input_sizes.tx

echo "Done."
