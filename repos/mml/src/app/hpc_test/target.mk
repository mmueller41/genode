TARGET = hpc_test
SRC_CC = trace_pfc.cc
LIBS += base posix libm libc stdcxx 
CC_OPT += -Wno-error -Wno-permissive -fpermissive -Wno-error=conversion

