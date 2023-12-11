#!/bin/bash
set -e
#SBATCH -n 32
#SBATCH --mem-per-cpu=4000
#SBATCH --job-name=blake3valgrind
#SBATCH --output=valgrind.out
#SBATCH --error=valgrind.err

module load gcc/9.3.0 papi/7.0.1
make benchmark_mp
OMP_NUM_THREADS=8 valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./benchmark
