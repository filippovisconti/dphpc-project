#!/bin/bash

#SBATCH --job-name="blake_mp_run"
#SBATCH --account="g34"
#SBATCH --mail-type=ALL
#SBATCH --mail-user=fvisconti@ethz.ch
#SBATCH --output=f_blake.out
#SBATCH --error=f_blake.err
#SBATCH --time=23:00:00
#SBATCH --partition=long
#SBATCH --nodes=1
#SBATCH --nodelist="ault18"
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=128
#SBATCH --mem=256G
#SBATCH --hint=nomultithread

export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

srun /scratch/fviscont/blake/benchmark
