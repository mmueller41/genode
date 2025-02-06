SRC_C = tcpecho_raw.c
SRC_CC = main.cpp
SRC_CC += vfs.cc printf.cc rand.cc sys_arch.cc
TARGET = lwip_raw
LIBS += base libm libc lwip 

CC_OLEVEL = -O3

VFS_DIR  = $(REP_DIR)/src/lib/vfs/lwip
vpath %.cc $(VFS_DIR) $(REP_DIR)/src/app/lwip_raw

LWIP_PORT_DIR := $(call select_from_ports,lwip)
LWIPDIR := $(LWIP_PORT_DIR)/src/lib/lwip/src
INC_DIR += $(LWIP_PORT_DIR)/include/lwip \
           $(LWIPDIR)/include \
           $(LWIPDIR)/include/ipv4 \
           $(LWIPDIR)/include/api \
           $(LWIPDIR)/include/netif \
           $(REP_DIR)/src/lib/lwip/include \
		   $(REP_DIR)/src/lib/vfs \
		   $(REP_DIR)/src/lib/vfs/lwip

CC_OPT += -Wno-error=conversion -Wno-error=effc++ 