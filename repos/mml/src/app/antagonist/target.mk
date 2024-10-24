MXINC_DIR=$(REP_DIR)/src/app/antagonist
GENODE_GCC_TOOLCHAIN_DIR ?= /usr/local/genode/tool/21.05

TARGET = stress_genode
# soure file for benchmark framework

SRC_CC = stress_linux.cc synthetic_worker.cc util.cc
LIBS += base libc stdcxx mxtasking 
EXT_OBJECTS += /usr/local/genode/tool/lib/clang/14.0.5/lib/linux/libclang_rt.builtins-x86_64.a /usr/local/genode/tool/lib/libatomic.a 
CUSTOM_CC = /usr/local/genode/tool/bin/clang
CUSTOM_CXX = /usr/local/genode/tool/bin/clang++
CC_OPT += --target=x86_64-genode --sysroot=/does/not/exist --gcc-toolchain=$(GENODE_GCC_TOOLCHAIN_DIR) -Wno-error -O2 -g -DNDEBUG -I$(MXINC_DIR) -std=c++20 #-D_GLIBCXX_ATOMIC_BUILTINS_8 -D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8
#CC_OPT +=  -femulated-tls -DCLANG_CXX11_ATOMICS
CC_CXX_WARN_STRICT =
CUSTOM_CXX_LIB := $(CROSS_DEV_PREFIX)g++
#CXX_LD += $(CROSS_DEV_PREFIX)g++ 