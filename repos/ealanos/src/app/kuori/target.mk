SRC_CC = main.cc
TARGET = kuori

INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/libc)/spec/x86_64

CC_OPT += -Wno-error=conversion -Wno-error=effc++
LIBS += base libm libc lwip stdcxx