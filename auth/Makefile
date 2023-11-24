FLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O1 -march=native -fopenmp
INCLUDES := -I include -lm
UNAME := $(shell uname -m)


ifeq ($(UNAME), arm64)
CC := gcc-13

test: clean
	$(CC) $(FLAGS) -g -o test myblake.c blake.c quick_test.c $(INCLUDES)
	./test

test_mp: clean
	$(CC) $(FLAGS) -DUSE_OPENMP -o test myblake.c blake.c quick_test.c $(INCLUDES)
	OMP_NUM_THREADS=16 ./test

blake_mp: clean
	@$(CC) $(FLAGS) -DMYBLAKE_MAIN -DUSE_OPENMP -g -o blake myblake.c $(INCLUDES)

blake: clean
	@$(CC) $(FLAGS) -DMYBLAKE_MAIN -g -o blake myblake.c $(INCLUDES)

run_mp: blake_mp
	OMP_NUM_THREADS=10 ./blake

run: blake
	./blake

benchmark: clean
	$(CC) $(FLAGS)  -o benchmark benchmark.c myblake.c blake.c sha256.c $(INCLUDES)
	./benchmark

benchmark_mp: clean
	$(CC) $(FLAGS) -DUSE_OPENMP -o benchmark benchmark.c myblake.c blake.c sha256.c $(INCLUDES)
	OMP_NUM_THREADS=16 ./benchmark

clean:
	@rm -fR main_arm blake main_arm.dSYM/ test test.dSYM/ blake.dSYM

else

# ------------- running on x86_64 -------------
CC := gcc
FLAGS += -DUSE_PAPI

PAPI_EVENTS:=PAPI_TOT_CYC
PAPI_OUTPUT_DIRECTORY="measurements"
PAPI_DEBUG=HIGHLEVEL
LIB=/cluster/apps/gcc-9.3.0/papi-7.0.1-k2lgc2loizuwvw2vzd4mopes22xhs7xs/lib
PAPI_EXPORT := LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) PAPI_EVENTS="${PAPI_EVENTS}" PAPI_OUTPUT_DIRECTORY=${PAPI_OUTPUT_DIRECTORY} PAPI_DEBUG=${PAPI_DEBUG}

INCLUDES := -I${CPATH} -I include
L=-L${LD_LIBRARY_PATH} -lm -lpapi


blake:
	$(CC) $(FLAGS) -o blake myblake.c $(INCLUDES)

print:
	python3 ${PAPI_ROOT}/bin/papi_hl_output_writer.py --source measurements/papi_* --notation=derived --type=detail > papi.log

benchmark: clean
	$(CC) $(FLAGS) $(INCLUDES) -o benchmark benchmark.c myblake.c blake.c sha256.c  $(L)
benchmark_mp: clean
	$(CC) $(FLAGS) -DUSE_OPENMP $(INCLUDES) -o benchmark benchmark.c myblake.c blake.c sha256.c $(L)
run_mp: benchmark_mp
	OMP_NUM_THREADS=16 $(PAPI_EXPORT) ./benchmark

run: benchmark
	$(PAPI_EXPORT) ./benchmark

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
REMOTE_HOST = euler
REMOTE_DIR = /cluster/home/${USER}
TO_COPY = Makefile *.c include *.json batch_script.sh gen_test_data.sh

ssh-copy:
	@rsync -avz $(TO_COPY) $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)

ssh-download:
	rsync -avz $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)/measurements/ ./papi_output

ssh-run:
	@ssh $(USER)@$(REMOTE_HOST) -t "sbatch batch_script.sh"
