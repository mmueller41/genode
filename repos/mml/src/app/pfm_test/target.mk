TARGET = pfm_test
SRC_CC = main.cc 
LIBS += base posix libm libc stdcxx libpfm4 
CC_OPT += -Wno-error -fpermissive -Wno-error=conversion