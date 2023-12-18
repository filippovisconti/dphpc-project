#!/bin/bash -l
#SBATCH --job-name="blake_short_run"
#SBATCH --account="blake"
#SBATCH --mail-type=ALL
#SBATCH --mail-user=fvisconti@ethz.ch
#SBATCH --time=00:30:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-core=1
#SBATCH --ntasks-per-node=1
#SBATCH --cpus-per-task=36
#SBATCH --mem=120GB
#SBATCH --partition=normal
#SBATCH --constraint=mc
#SBATCH --hint=nomultithread

export OMP_NUM_THREADS=$SLURM_CPUS_PER_TASK

srun ./benchmark
