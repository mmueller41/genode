TARGET = hello_mxtask
SRC_CC = main.cc 
LIBS += base libc stdcxx mxtasking     
CC_OPT += -Wno-error -fno-aligned-new -g
CC_OLEVEL = -O0
CC_CXX_WARN_STRICT =
