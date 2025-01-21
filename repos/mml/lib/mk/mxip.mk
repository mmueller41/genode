#
# lwIP TCP/IP library
#
# The library implements TCP and UDP as well as DNS and DHCP.
#

LWIP_PORT_DIR := $(call select_from_ports,mxip)
LWIPDIR := $(LWIP_PORT_DIR)/src/lib/lwip/src

-include $(LWIPDIR)/Filelists.mk

# Genode platform files
SRC_CC = printf.cc rand.cc sys_arch.cc mxnic_netif.cc

# Core files
SRC_C += $(notdir $(COREFILES))

# IPv4 files
SRC_C += $(notdir $(CORE4FILES))

# IPv6 files
SRC_C += $(notdir $(CORE6FILES))

# Network interface files
SRC_C += $(notdir $(NETIFFILES))

INC_DIR += $(REP_DIR)/include/mxip \
           $(LWIP_PORT_DIR)/include/lwip \
           $(LWIPDIR)/include \
           $(LWIPDIR)/include/ipv4 \
           $(LWIPDIR)/include/api \
           $(LWIPDIR)/include/netif \

vpath %.cc $(REP_DIR)/src/lib/mxip/platform
vpath %.c  $(sort $(dir \
	$(COREFILES) $(CORE4FILES) $(CORE6FILES) $(NETIFFILES)))

GENODE_GCC_TOOLCHAIN_DIR ?= /usr/local/genode/tool/21.05

CUSTOM_CXX = /usr/local/genode/tool/bin/clang++
CUSTOM_CC = /usr/local/genode/tool/bin/clang

CC_OPT := --target=x86_64-genode --sysroot=/does/not/exist --gcc-toolchain=$(GENODE_GCC_TOOLCHAIN_DIR) -DCLANG_CXX11_ATOMICS -Wno-error=all -Wno-error=conversion -Wno-error=effc++ -Wno-error=unknown-attributes -g -DNDEBUG -I$(MXINC_DIR) -std=c++20 -mssse3 #-D_GLIBCXX_ATOMIC_BUILTINS_8 -D__GCC_HAVE_SYNC_COMPARE_AND_SWAP_8

CC_OLEVEL = -O3

LIBS += libm libc stdcxx mxtasking
EXT_OBJECTS += /usr/local/genode/tool/lib/clang/14.0.5/lib/linux/libclang_rt.builtins-x86_64.a /usr/local/genode/tool/lib/libatomic.a 

