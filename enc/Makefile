UNAME := $(shell uname)
INCLUDES := -I/usr/local/include
LDFLAGS := -L/usr/local/lib -lsodium
TARGET := bin
F_POS := input_data/
FILES := ./src/chacha.cpp main.cpp ./src/test_client.cpp
FILE_CHACHA := ./src/chacha.cpp
FILE_CHACHA_O := ./src/chacha.o
CXXFLAGS := -D $(UNAME) -Wall -Wextra -pedantic -std=c++11 -ffast-math -O3 -mfma -mavx2 -mavx -march=native -fopenmp
OBJS := $(FILES:.cpp=.o)
VER ?= 0

ifeq ($(UNAME), Darwin)
	CXX := g++-13
else
	CXX := g++
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(CXXFLAGS) $(LDFLAGS) $(INCLUDES)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(INCLUDES)

s_run: $(TARGET) check-env check-files
	./$(TARGET) $(LEN) $(F_POS)input_$(LEN).txt $(VER)

m_run: $(TARGET) check-env
	./$(TARGET) $(LEN)
	@if [ $$? -eq 0 ]; then \
	    printf "\n\033[1;32mRun ended without errors.\033[0m\033[1m\n\nPlotting Results..."; \
	else \
	    printf "\033[1;31mError: The run failed.\033[0m\n"; exit 1; \
	fi
	@python3 -m venv venv; . ./venv/bin/activate; pip install --upgrade -q pip; pip install -q matplotlib; python3 ./output_data/graphs.py;
	@if [ $$? -eq 0 ]; then \
	    printf "\r\033[1;32mResults Plotted.    \033[0m\n\n"; \
	else \
	    printf "\033[1;31mError plotting results\033[0m\n"; exit 1; \
	fi

callgrind: $(TARGET) check-env check-files
	mkdir -p callgrind
	valgrind --tool=callgrind --simulate-cache=yes --callgrind-out-file=callgrind/callgrind_$(LEN)_opt$(VER) ./$(TARGET) $(LEN) $(F_POS)input_$(LEN).txt $(VER)

valgrind: $(TARGET) check-env check-files
	valgrind --leak-check=yes --track-origins=yes ./$(TARGET) $(LEN) $(F_POS)input_$(LEN).txt $(VER)

check-no-inline: clean
	$(CXX) $(CXXFLAGS) -fopt-info-inline-missed -c $(FILE_CHACHA) -o $(FILE_CHACHA_O)  $(INCLUDES)

check-no-vectorizazion: clean
	$(CXX) $(CXXFLAGS) -fopt-info-vec-missed -c $(FILE_CHACHA) -o $(FILE_CHACHA_O)  $(INCLUDES)

check-no-loop-unrolling: clean
	$(CXX) $(CXXFLAGS) -fopt-info-loop-missed -c $(FILE_CHACHA) -o $(FILE_CHACHA_O)  $(INCLUDES)

clean:
	rm -fR bin $(OBJS)

clean-all: clean
	rm -fR input_data bin.dSYM

check-env:
	@if [ -n "$(OMP_NUM_THREADS)" ]; then \
		printf "\033[1;32mThread number set up correctly.\033[0m\n\n"; \
	else \
		printf "\033[1;33mERROR\033[0m\n\033[33mThe number of threads is not set. Please do: \n\nsource num_thread_definer.sh\033[0m\n\n"; \
	fi

check-files:
	@MISSING_FILE=$(if $(wildcard $(F_POS)input_$(LEN).txt),,$(LEN)); \
	MISSING_COUNT=`echo "$$MISSING_FILE" | awk '{print NF}'`; \
	if [ $$MISSING_COUNT -gt 0 ]; then \
    	printf "\033[1;33mMissing file!\033[0m\n"; \
    	printf "Generating input data...\n"; \
    	mkdir -p $(F_POS); \
    	base64 -i /dev/urandom | head -c $(LEN) > $(F_POS)input_$(LEN).txt; \
		printf "\033[1;32mDone\033[0m\n\n"; \
	else \
    	printf "\033[1;32mInput file is present.\033[0m\n\n"; \
	fi