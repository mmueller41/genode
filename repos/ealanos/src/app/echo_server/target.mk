MXINC_DIR=$(REP_DIR)/src/app/echo_server
MXINC_DIR+=-I$(REP_DIR)/src/app/blinktree
GENODE_GCC_TOOLCHAIN_DIR ?= /usr/local/genode/tool/23.05
MXBENCH_DIR=$(REP_DIR)/src/lib

TARGET = blinktree_daemon
# soure file for benchmark framework

# source files for blinktree benchmark
SRC_BTREE += main.cpp
SRC_BTREE += server.cpp

INC_DIR += /usr/local/genode/tool/lib/clang/14.0.5/include/
INC_DIR += $(REP_DIR)/src/lib
INC_DIR += $(REP_DIR)/include
INC_DIR += $(REP_DIR)/include/ealanos/util
INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/libc)/spec/x86_64
vpath %.h ${INC_DIR}
LD_OPT += --allow-multiple-definition

SRC_CC = ${SRC_MXBENCH} ${SRC_BTREE}
LIBS += base libc stdcxx mxtasking mxip
EXT_OBJECTS +=  /usr/local/genode/tool/lib/libatomic.a /usr/local/genode/tool/23.05/lib/gcc/x86_64-pc-elf/12.3.0/libgcc_eh.a /usr/local/genode/tool/lib/clang/14.0.5/lib/linux/libclang_rt.builtins-x86_64.a 
CUSTOM_CC = /usr/local/genode/tool/bin/clang
CUSTOM_CXX = /usr/local/genode/tool/bin/clang++
CC_OPT := --target=x86_64-genode --sysroot=/does/not/exist --gcc-toolchain=$(GENODE_GCC_TOOLCHAIN_DIR) -Wno-error -g -DNDEBUG -I$(MXINC_DIR) -std=c++20 #-D_GLIBCXX_ATOMIC_BUILTINS_8 -D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
CC_OPT +=  -I$(MXBENCH_DIR)
CC_OLEVEL = -O3
CC_CXX_WARN_STRICT =
CUSTOM_CXX_LIB := $(CROSS_DEV_PREFIX)g++
#CXX_LD += $(CROSS_DEV_PREFIX)g++ 
