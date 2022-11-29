MXTASKING_DIR := $(call select_from_ports,mxtasking)/src/lib/mxtasking
GENODE_GCC_TOOLCHAIN_DIR := /usr/local/genode/tool/21.05

SRC_CC = $(shell find $(MXTASKING_DIR)/src/mx -name '*.cpp')
vpath %.cpp $(MXTASKING_DIR)/src/mx

INC_DIR += $(MXTASKING_DIR)/src $(MXTASKING_DIR)/lib
vpath %.h ${INC_DIR}

CUSTOM_CXX = /usr/local/genode/tool/bin/clang++
CUSTOM_CC = /usr/local/genode/tool/bin/clang

CC_OPT += --target=x86_64-genode --sysroot=/does/not/exist --gcc-toolchain=$(GENODE_GCC_TOOLCHAIN_DIR) -DCLANG_CXX11_ATOMICS 
CC_OPT += -std=c++17 -pedantic -Wall \
 -Wno-invalid-offsetof -Wcast-align -Wcast-qual -Wctor-dtor-privacy -Wdisabled-optimization \
 -Wformat=2 -Winit-self -Wmissing-declarations -Wmissing-include-dirs -Woverloaded-virtual \
 -Wredundant-decls -Wshadow -Wsign-promo -Wstrict-overflow=5 -Wswitch-default -Wundef \
 -Wno-unused -Wold-style-cast -Wno-uninitialized -O2 -g -DNDEBUG -fno-aligned-new

CC_OPT += $(addprefix -I ,$(INC_DIR)) 
CC_CXX_WARN_STRICT =

LIBS += base libm libc stdcxx
EXT_OBJECTS += /usr/local/genode/tool/lib/clang/14.0.5/lib/linux/libclang_rt.builtins-x86_64.a /usr/local/genode/tool/lib/libatomic.a 
#SHARED_LIB = yes
