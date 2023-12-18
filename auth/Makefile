FLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O3 -march=native
# FLAGS+=-DDEBUG

INCLUDES := -I include
LIBS := -lm
UNAME := $(shell uname -m)
CC := icc
SRC =  blake_f.c blake_d.c blake_ref.c utils.c

TEST_SOURCES := $(SRC) quick_test.c
BLAKE_SOURCES := blake_f.c utils.c
BENCHMARK_SOURCES := benchmark.c $(SRC) sha256.c
NUM_THREADS := 32

ifeq ($(UNAME), arm64)
CC := gcc-13
FLAGS += -fopenmp

else
FLAGS += -qopenmp -DUSE_PAPI
INCLUDES += -I$(CPATH)
#maybe CPATH not needed
PAPI_EVENTS := PAPI_TOT_CYC
PAPI_OUTPUT_DIRECTORY := "measurements"
PAPI_DEBUG := HIGHLEVEL
PAPI_EXPORT := PAPI_EVENTS="$(PAPI_EVENTS)" LD_LIBRARY_PATH=$(LD_LIBRARY_PATH)
PAPI_EXPORT += PAPI_OUTPUT_DIRECTORY=$(PAPI_OUTPUT_DIRECTORY) PAPI_DEBUG=${PAPI_DEBUG}
L += -lpapi
endif

# ------------ data generation ---------------
gen_sh:
	@mkdir -p input_data
	bash gen.sh

gen_py:
	@mkdir -p input_data
	python3 gen.py

%_mp: FLAGS += -DUSE_OPENMP
# tests
test test_mp: FLAGS += -DBENCHMARK
test test_mp: clean
	$(CC) $(TEST_SOURCES) $(FLAGS) -o test $(INCLUDES) $(L)

run_test:
	OMP_NUM_THREADS=$(NUM_THREADS) ./test

# benchmarks
benchmark benchmark_mp: FLAGS += -DBENCHMARK
benchmark benchmark_mp: clean
	$(CC) $(BENCHMARK_SOURCES) -o benchmark $(FLAGS) $(INCLUDES) $(L)

run_benchmark:
	OMP_NUM_THREADS=$(NUM_THREADS) $(PAPI_EXPORT) ./benchmark



# run implementations singularly



clean:
	@rm -fR blake *.dSYM/ test benchmark main slurm-*
# ------------ remote host interaction -----------
USER=fvisconti
REMOTE_HOST := euler
REMOTE_DIR := /cluster/home/${USER}/blake
TO_COPY := Makefile *.c include *.json *.sh gen.py input_sizes.txt

ssh-copy:
	@rsync -avz $(TO_COPY) $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)

ssh-download:
	rsync -avz $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)/measurements/ ./papi_output

ssh-run:
	@ssh $(USER)@$(REMOTE_HOST) -t "sbatch batch_script.sh"
