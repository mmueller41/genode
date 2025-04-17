SRC_CC += launcher
TARGET = launcher

INC_DIR += $(call select_from_repositories,src/lib/libc)/spec/x86_64
INC_DIR += $(call select_from_repositories,src/lib/libc)

LIBS = base libc stdcxx
