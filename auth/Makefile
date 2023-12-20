FLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O3 -march=native
INCLUDES := -I include
L := -lm
UNAME := $(shell uname -m)
SRC_DIR := src
SRC =  blake_f.c blake_d.c blake_ref.c utils.c
SRC := $(addprefix $(SRC_DIR)/, $(SRC))

TEST_SOURCES := $(SRC) $(SRC_DIR)/quick_test.c
BLAKE_SOURCES := blake_d.c blake_f.c utils.c main.c
BLAKE_SOURCES := $(addprefix $(SRC_DIR)/, $(BLAKE_SOURCES))

BENCHMARK_SOURCES := $(SRC_DIR)/{benchmark.c,sha256.c} $(SRC)
NUM_THREADS := 64

FLAGS += -fopenmp
ifeq ($(UNAME), arm64)
CC := gcc-13
else
CC := gcc
# FLAGS += -qopenmp
# LD_LIBRARY_PATH=/opt/intel/composer_xe_2011_sp1.7.256/compiler/lib/intel64:$LD_LIBRARY_PATH
endif

# ------------ data generation ---------------
gen_sh:
	@mkdir -p input_data
	bash gen.sh

gen_py:
	@mkdir -p input_data
	python3 gen.py

%_mp: FLAGS += -DUSE_OPENMP

# ------------ tests
# to silence prints
test test_mp: FLAGS += -DBENCHMARK
test test_mp: clean
	$(CC) $(TEST_SOURCES) $(FLAGS) -o test $(INCLUDES) $(L)

run_test:
	OMP_NUM_THREADS=$(NUM_THREADS) ./test

# ------------ benchmarks
benchmark benchmark_mp: FLAGS += -DBENCHMARK
benchmark benchmark_mp:
	$(CC) $(BENCHMARK_SOURCES) -o benchmark $(FLAGS) $(INCLUDES) $(L)

run_benchmark:
	OMP_NUM_THREADS=$(NUM_THREADS) ./benchmark

# ------------ main, combined implementation, auto selection
blake blake_mp: clean
	$(CC) $(BLAKE_SOURCES) $(FLAGS) -o blake $(INCLUDES) $(L)

run_main:
	OMP_NUM_THREADS=$(NUM_THREADS) ./blake

# ------------ run implementations singularly
blake_f: FLAGS += -DMYBLAKE_MAIN
blake_d: FLAGS += -DDBLAKE_MAIN
blake_f blake_d: clean
	$(CC)  $@.c  utils.c $(FLAGS) -o main $(INCLUDES)

clean:
	@rm -fR blake *.dSYM/ test benchmark main slurm-*
# ------------ remote host interaction -----------
USER=fvisconti
REMOTE_HOST := euler
REMOTE_DIR := /cluster/home/${USER}/blake
TO_COPY := Makefile *.c include *.sh gen.py
#input_sizes.txt

ssh-copy:
	@rsync -avz $(TO_COPY) $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)

ssh-download:
	rsync -avz $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)/measurements/ ./papi_output

ssh-run:
	@ssh $(USER)@$(REMOTE_HOST) -t "sbatch batch_script.sh"


# ------------ AULT ------------------------------
AULT_HOST := ault-ela
AULT_USER := fviscont
AULT_DIR := /scratch/${AULT_USER}/blake

ault-copy:
	@rsync -avz $(TO_COPY) $(AULT_USER)@$(AULT_HOST):$(AULT_DIR)

ault-download:
	rsync -avz $(AULT_USER)@$(AULT_HOST):$(AULT_DIR)/output_data/ .
ault-bash:
	srun -n1 -N1 -w ault02 --time=12:00:00 --mem=300G --pty bash
