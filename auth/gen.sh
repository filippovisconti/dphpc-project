#!/bin/bash
set -e

echo "Generating test data..."
input="./input_sizes.txt"

mkdir -p input_data
while read -r line
do
  #skip empty lines
  [[ -z "$line" ]] && continue
  #skip line starting with #
  [[ $line = \#* ]] && echo "Skipping" && continue
  fsize=$(echo "$line" | grep -o '[0-9]\+')
  unit=$(echo "$line" | grep -o '[a-zA-Z]\+')
  base="1"
  if [ "$unit" = "KB" ]; then
    fsize=$(($fsize * 1024))
    base="1K"
  elif [ "$unit" = "MB" ]; then
    fsize=$(($fsize * 1024 * 1024))
    base="1M"
  elif [ "$unit" = "GB" ]; then
    fsize=$(($fsize * 1024 * 1024 * 1024))
    base="1G"
  fi

  dd if=/dev/urandom of=./input_data/$line bs=$base count=$fsize

  echo "$line"
done < "$input"

# for line in file input_sizes.tx

echo "Done."
