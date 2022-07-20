MXINC_DIR=$(REP_DIR)/src/app/blinktree

TARGET = blinktree
# soure file for benchmark framework
SRC_MXBENCH = benchmark/workload_set.cpp
SRC_MXBENCH += benchmark/workload.cpp
SRC_MXBENCH += benchmark/cores.cpp
#SRC_MXBENCH += benchmark/perf.cpp
SRC_MXBENCH += benchmark/string_util.cpp
# source files for blinktree benchmark
SRC_BTREE += blinktree_benchmark/main.cpp
SRC_BTREE += blinktree_benchmark/benchmark.cpp

SRC_CC = ${SRC_MXBENCH} ${SRC_BTREE}
LIBS += base libc stdcxx mxtasking     
CC_OPT += -Wno-error -fno-aligned-new -I$(MXINC_DIR)
CC_CXX_WARN_STRICT =
