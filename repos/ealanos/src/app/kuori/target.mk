SRC_CC = main.cc
SRC_CC += vfs.cc printf.cc rand.cc sys_arch.cc
TARGET = kuori

LIBPORTS_DIR = $(REP_DIR)/../libports
VFS_DIR  = $(LIBPORTS_DIR)/src/lib/vfs/lwip
vpath %.cc $(VFS_DIR)


LWIP_PORT_DIR := $(call select_from_ports,lwip)
LWIPDIR := $(LWIP_PORT_DIR)/src/lib/lwip/src
INC_DIR += $(LWIP_PORT_DIR)/include/lwip \
           $(LWIPDIR)/include \
           $(LWIPDIR)/include/ipv4 \
           $(LWIPDIR)/include/api \
           $(LWIPDIR)/include/netif \
           $(LIBPORTS_DIR)/src/lib/lwip/include \
		   $(LIBPORTS_DIR)/src/lib/vfs \
		   $(LIBPORTS_DIR)/src/lib/vfs/lwip

CC_OPT += -Wno-error=conversion -Wno-error=effc++
LIBS += base libm libc lwip stdcxx