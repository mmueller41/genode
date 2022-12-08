TARGET = libpfm_test
SRC_CC = check_events.c
LIBS += base posix libm libc stdcxx libpfm4 
CC_OPT += -Wno-error -Wno-permissive -fpermissive

