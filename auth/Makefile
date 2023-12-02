FLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O3 -march=native -fopenmp
INCLUDES := -I include -lm
UNAME := $(shell uname -m)
CC := gcc
TEST_SOURCES := myblake.c blake.c quick_test.c
BLAKE_SOURCES := myblake.c
BENCHMARK_SOURCES := benchmark.c myblake.c blake.c sha256.c
NUM_THREADS := 32


ifeq ($(UNAME), arm64)
CC := gcc-13

test: clean
	$(CC) $(FLAGS) -g -o test $(TEST_SOURCES) $(INCLUDES)
	./test

test_mp: clean
	$(CC) $(FLAGS) -DUSE_OPENMP -o test $(TEST_SOURCES) $(INCLUDES)
	OMP_NUM_THREADS=$(NUM_THREADS) ./test

blake: clean
	@$(CC) $(FLAGS) -DMYBLAKE_MAIN -g -o blake $(BLAKE_SOURCES) $(INCLUDES)

blake_mp: clean
	@$(CC) $(FLAGS) -DMYBLAKE_MAIN -DUSE_OPENMP -g -o blake $(BLAKE_SOURCES) $(INCLUDES)

run: blake
	./blake

run_mp: blake_mp
	OMP_NUM_THREADS=$(NUM_THREADS) ./blake

benchmark: clean
	$(CC) $(FLAGS) -DBENCHMARK -o benchmark $(BENCHMARK_SOURCES) $(INCLUDES)
	./benchmark

benchmark_mp: clean
	$(CC) $(FLAGS) -DBENCHMARK -DUSE_OPENMP -o benchmark $(BENCHMARK_SOURCES) $(INCLUDES)
	OMP_NUM_THREADS=$(NUM_THREADS) ./benchmark

clean:
	@rm -fR blake *.dSYM/ test benchmark

else

# ------------- running on x86_64 -------------
FLAGS += -DUSE_PAPI
PAPI_EVENTS := PAPI_TOT_CYC
PAPI_OUTPUT_DIRECTORY := "measurements"
PAPI_DEBUG := HIGHLEVEL
PAPI_EXPORT := LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) PAPI_EVENTS="$(PAPI_EVENTS)"
PAPI_EXPORT += PAPI_OUTPUT_DIRECTORY=$(PAPI_OUTPUT_DIRECTORY) PAPI_DEBUG=${PAPI_DEBUG}

INCLUDES := -I${CPATH} -I include
L := -L${LD_LIBRARY_PATH} -lm -lpapi

blake:
	$(CC) $(FLAGS) -o blake $(BLAKE_SOURCES) $(INCLUDES)

print:
	python3 ${PAPI_ROOT}/bin/papi_hl_output_writer.py --source measurements/papi_* --notation=derived --type=detail > papi.log

benchmark: clean
	$(CC) $(FLAGS) -DBENCHMARK $(INCLUDES) -o benchmark $(BENCHMARK_SOURCES) $(L)

benchmark_mp: clean
	$(CC) $(FLAGS) -DBENCHMARK -DUSE_OPENMP $(INCLUDES) -o benchmark $(BENCHMARK_SOURCES) $(L)

run: benchmark
	$(PAPI_EXPORT) ./benchmark

run_mp: benchmark_mp
	OMP_NUM_THREADS=$(NUM_THREADS) $(PAPI_EXPORT) ./benchmark

compile: clean
	echo "Compiling with PAPI on for x86_64"
	$(CC) $(FLAGS) -o main $(INCLUDES) -lpapi $(FILES)

debug: clean
	$(CC) $(FLAGS) -ggdb -g -o main $(INCLUDES) -lpapi $(FILES)

clean:
	rm -fR main benchmark

endif

all: blake

gen:
	sh gen_test_data.sh

# set USER to your username
# run like e.g.: USER=fvisconti make ssh-copy
REMOTE_HOST := euler
REMOTE_DIR := /cluster/home/${USER}
TO_COPY := Makefile *.c include *.json batch_script.sh gen_test_data.sh

ssh-copy:
	@rsync -avz $(TO_COPY) $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)

ssh-download:
	rsync -avz $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)/measurements/ ./papi_output

ssh-run:
	@ssh $(USER)@$(REMOTE_HOST) -t "sbatch batch_script.sh"
