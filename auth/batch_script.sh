#!/bin/bash

#SBATCH -n 32
#SBATCH --mem-per-cpu=2000
#SBATCH --job-name=blake3_sha256
#SBATCH --output=blake.out
#SBATCH --error=blake.err


module load gcc/9.3.0 papi/7.0.1
make run_mp
