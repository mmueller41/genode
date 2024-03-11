TARGET = portal_experiment
SRC_CC = main.cc
LIBS = base libc stdcxx
INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/libc)/spec/x86_64
ifdef HYPERCALL
CC_OPT += -DHYPERCALL
endif