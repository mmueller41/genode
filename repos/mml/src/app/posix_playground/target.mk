TARGET = posix_playground
SRC_CC = main.cc
LIBS += base posix libm libc stdcxx 
CC_OPT += -Wno-error -Wno-permissive -fpermissive

