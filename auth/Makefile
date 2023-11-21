FLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O0 -march=native
INCLUDES := -I include -lm -fopenmp
UNAME := $(shell uname -m)

all: blake

# FILES:= $(shell find . -name \*.c)
FILES:= blake.c benchmark.c sha256.c
ifeq ($(UNAME), arm64)
CC := gcc-13

test: clean
	$(CC) $(FLAGS) -g -o test myblake.c blake.c quick_test.c $(INCLUDES)
	./test

test_mp: clean
	$(CC) $(FLAGS) -DUSE_OPENMP -o test myblake.c blake.c quick_test.c $(INCLUDES)
	OMP_NUM_THREADS=10 ./test

blake_mp: clean
	@$(CC) $(FLAGS) -DMYBLAKE_MAIN -DUSE_OPENMP -g -o blake myblake.c $(INCLUDES)

blake: clean
	@$(CC) $(FLAGS) -DMYBLAKE_MAIN -g -o blake myblake.c $(INCLUDES)
	./blake

run: blake_mp
	OMP_NUM_THREADS=10 ./blake

clean:
	@rm -fR main_arm blake main_arm.dSYM/ test test.dSYM/ blake.dSYM

else
CC := gcc
FLAGS += -DUSE_PAPI

PAPI_EVENTS:="PAPI_TOT_INS,PAPI_TOT_CYC,PAPI_VEC_SP,PAPI_VEC_DP"
PAPI_OUTPUT_DIRECTORY="measurements"
PAPI_DEBUG=HIGHLEVEL
PAPI_EXPORT := LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) PAPI_EVENTS="${PAPI_EVENTS}" PAPI_OUTPUT_DIRECTORY=${PAPI_OUTPUT_DIRECTORY} PAPI_DEBUG=${PAPI_DEBUG}

INCLUDES := -I${CPATH} -I include -L${LD_LIBRARY_PATH} -lm


blake:
	$(CC) $(FLAGS) -o blake myblake.c $(INCLUDES)

print:
	python3 ${PAPI_ROOT}/bin/papi_hl_output_writer.py --source measurements/papi_hl_output --notation=derived --type=summary > papi.log

run: debug
	$(PAPI_EXPORT) ./main


compile: clean
	echo "Compiling with PAPI on for x86_64"
	$(CC) $(FLAGS) -o main $(INCLUDES) -lpapi $(FILES)

debug: clean
	$(CC) $(FLAGS) -ggdb -g -o main $(INCLUDES) -lpapi $(FILES)

clean:
	rm -fR main papi_output/*

endif

USER=fvisconti
REMOTE_HOST = euler
REMOTE_DIR = /cluster/home/${USER}
TO_COPY = Makefile *.c *.txt include *.json batch_script.sh gen_test_data.sh

ssh-copy:
	@rsync -avz $(TO_COPY) $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)

ssh-download:
	@rsync -avz $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)/papi_output ./papi_output

ssh-debug:
	# module load gcc/9.3.0 papi/7.0.1
	# export OMP_NUM_THREADS=30
	@ssh $(USER)@$(REMOTE_HOST) -t "OMP_NUM_THREADS=32 sbatch batch_script.sh"


# gprof:
# 	rm -f gmon.out perf.data* main_adv analysis.txt
# 	gcc -O3 -march=native -pg -o main_adv operator_tree.c tree_adv.c tree_helpers.c -I -lm
# 	taskset --cpu-list 0 ./main_adv
# 	gprof main_adv gmon.out > analysis.txt
