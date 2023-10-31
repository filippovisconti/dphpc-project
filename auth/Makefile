CC := gcc
FLAGS := -Wall -Wextra -Werror -pedantic -std=c99 -O3 -march=native
INCLUDES := -I include -lm
UNAME := $(shell uname -m)

DEPTH := 23
ITERATIONS := 5
FUN_NUMBER := 3
all: compile

FILES:= $(shell find . -name \*.c)
ifeq ($(UNAME), arm64)

run: compile 
	./main_arm $(DEPTH) $(ITERATIONS)

compile: clean
	@echo "No PAPI support for arm64"
	$(CC) $(FLAGS) -o main_arm $(FILES) $(INCLUDES)

debug:
	$(CC) $(FLAGS) -g -o main_arm $(FILES) $(INCLUDES)
	
clean:
	rm -fR main_arm

else

CPATH :="/cluster/apps/gcc-4.8.5/papi-5.5.1-zirk332p6tlx6odos5kq5sem37fpe5qj/include"
PAPI_ROOT := "/cluster/apps/gcc-4.8.5/papi-5.5.1-zirk332p6tlx6odos5kq5sem37fpe5qj"

PAPI_EVENTS:="PAPI_TOT_INS,PAPI_TOT_CYC,PAPI_VEC_SP,PAPI_VEC_DP"
PAPI_OUTPUT_DIRECTORY="scratch/measurement"
PAPI_DEBUG=HIGHLEVEL
PAPI_EXPORT := LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) PAPI_EVENTS="${PAPI_EVENTS}" PAPI_OUTPUT_DIRECTORY=${PAPI_OUTPUT_DIRECTORY} PAPI_DEBUG=${PAPI_DEBUG}

INCLUDES := -I${CPATH} -L${LD_LIBRARY_PATH} -lm

print:
	python3 ${PAPI_DIR}/bin/papi_hl_output_writer.py  --notation=derived --type=summary > papi.log 
	
run: compile
	$(PAPI_EXPORT) taskset --cpu-list 0 ./main 


compile: clean
	echo "Compiling with PAPI on for x86_64"
	$(CC) $(FLAGS) -o main $(FILES) $(INCLUDES) -lpapi

debug: clean
	$(CC) $(FLAGS) -g -o main $(FILES) $(INCLUDES) -lpapi
	
clean:
	rm -fR main papi_output/* 

endif

USER=fvisconti
REMOTE_HOST = euler
REMOTE_DIR = /cluster/home/${USER}
TO_COPY = ./Makefile *.c *.h *.txt

ssh-copy:
	@rsync -avz $(TO_COPY) $(USER)@$(REMOTE_HOST):$(REMOTE_DIR)

ssh-debug:
	@ssh $(USER)@$(REMOTE_HOST) -t "module load papi && srun make run"


# gprof:
# 	rm -f gmon.out perf.data* main_adv analysis.txt
# 	gcc -O3 -march=native -pg -o main_adv operator_tree.c tree_adv.c tree_helpers.c -I -lm
# 	taskset --cpu-list 0 ./main_adv
# 	gprof main_adv gmon.out > analysis.txt