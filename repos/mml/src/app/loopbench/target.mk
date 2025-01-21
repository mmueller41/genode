TARGET = loopbench
SRC_CC = loop_bench_mxtasking.cpp \
		loop.c \
		bench.c \
		profiling.c
		
LIBS += base libc stdcxx mxtasking     
CC_OPT += -Wno-error -fno-aligned-new -g
CC_OLEVEL = -O0
CC_CXX_WARN_STRICT =
