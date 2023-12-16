#!/bin/bash

#SBATCH -n 32
#SBATCH --mem-per-cpu=2000
#SBATCH --job-name=blake3_sha256
#SBATCH --output=blake.out
#SBATCH --error=blake.err


module load gcc/11.4.0 papi/7.0.1
make test_mp
