#!/bin/bash

#SBATCH --job-name="blake_mp_run"
#SBATCH --account="g34"
#SBATCH --mail-type=ALL
#SBATCH --mail-user=fvisconti@ethz.ch
#SBATCH --output=blake.out
#SBATCH --error=blake.err
#SBATCH --time=04:00:00
#SBATCH --nodes=1
#SBATCH --nodelist="ault09"
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=64
#SBATCH --hint=nomultithread

export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

# module load intel/compiler
srun /scratch/fviscont/blake/benchmark
