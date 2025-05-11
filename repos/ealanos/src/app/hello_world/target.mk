TARGET = hello_mxtask
SRC_CC = main.cc 
LIBS += base libm libc stdcxx mxtasking     

INC_DIR += $(REP_DIR)/src/lib
INC_DIR += $(REP_DIR)/include
INC_DIR += $(REP_DIR)/include/ealanos/util
INC_DIR += $(call select_from_repositories,src/lib/libc)
INC_DIR += $(call select_from_repositories,src/lib/libc)/spec/x86_64
vpath %.h ${INC_DIR}

CC_CXX_WARN_STRICT =
CC_OPT += $(addprefix -I, $(INC_DIR))
CUSTOM_CXX_LIB := $(CROSS_DEV_PREFIX)g++
CUSTOM_CXX = /usr/local/genode/tool/bin/clang++
CUSTOM_CC = /usr/local/genode/tool/bin/clang
GENODE_GCC_TOOLCHAIN_DIR := /usr/local/genode/tool/23.05

LD_OPT += --allow-multiple-definition

CC_OPT += --target=x86_64-genode --sysroot=/does/not/exist --gcc-toolchain=$(GENODE_GCC_TOOLCHAIN_DIR) -DCLANG_CXX11_ATOMICS --rtlib=libgcc -DCLANG_DEFAULT_UNWINDLIB="libunwind" -femulated-tls
CC_OPT += -std=c++20 -pedantic -Wall \
 -Wno-invalid-offsetof -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization \
 -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Woverloaded-virtual \
 -Wredundant-decls -Wshadow -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef \
 -Wno-unused -Wold-style-cast -Wno-uninitialized -O2 -g
EXT_OBJECTS +=  /usr/local/genode/tool/lib/libatomic.a /usr/local/genode/tool/23.05/lib/gcc/x86_64-pc-elf/12.3.0/libgcc_eh.a /usr/local/genode/tool/lib/clang/14.0.5/lib/linux/libclang_rt.builtins-x86_64.a 
