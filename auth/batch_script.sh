#!/bin/bash

#SBATCH -n 30
#SBATCH --mem-per-cpu=2000
#SBATCH --job-name=blake3_sha256
#SBATCH --output=a.out
#SBATCH --error=a.err

module load gcc/9.3.0 papi/7.0.1
sh gen_test_data.sh
make run >/cluster/home/fvisconti/make_stout.txt
