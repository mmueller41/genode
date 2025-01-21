LIBPFM4_DIR := $(call select_from_ports,libpfm4)/src/lib/libpfm4

CC_OPT += -D_REENTRANT -fvisibility=hidden

SRC_CC = $(LIBPFM4_DIR)/lib/pfmlib_common.c

# build libpfm only for x86_64 for now
CONFIG_PFMLIB_ARCH_X86_64=y
CONFIG_PFMLIB_ARCH_X86=y

CONFIG_PFMLIB_SHARED?=n
CONFIG_PFMLIB_DEBUG?=y
CONFIG_PFMLIB_NOPYTHON?=y

#
# list all library support modules
#
ifeq ($(CONFIG_PFMLIB_ARCH_IA64),y)
INCARCH = $(INC_IA64)
#SRCS   += pfmlib_gen_ia64.c pfmlib_itanium.c pfmlib_itanium2.c pfmlib_montecito.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_IA64
endif

ifeq ($(CONFIG_PFMLIB_ARCH_X86),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_intel_x86_perf_event.c pfmlib_amd64_perf_event.c \
	pfmlib_intel_netburst_perf_event.c \
	pfmlib_intel_snbep_unc_perf_event.c
endif

INCARCH = $(INC_X86)
SRCS   += pfmlib_amd64.c pfmlib_intel_core.c pfmlib_intel_x86.c \
	  pfmlib_intel_x86_arch.c pfmlib_intel_atom.c \
	  pfmlib_intel_nhm_unc.c pfmlib_intel_nhm.c \
	  pfmlib_intel_wsm.c  \
	  pfmlib_intel_snb.c pfmlib_intel_snb_unc.c \
	  pfmlib_intel_ivb.c pfmlib_intel_ivb_unc.c \
	  pfmlib_intel_hsw.c \
	  pfmlib_intel_bdw.c \
	  pfmlib_intel_skl.c \
	  pfmlib_intel_icl.c \
	  pfmlib_intel_spr.c \
	  pfmlib_intel_rapl.c \
	  pfmlib_intel_snbep_unc.c \
	  pfmlib_intel_snbep_unc_cbo.c \
	  pfmlib_intel_snbep_unc_ha.c \
	  pfmlib_intel_snbep_unc_imc.c \
	  pfmlib_intel_snbep_unc_pcu.c \
	  pfmlib_intel_snbep_unc_qpi.c \
	  pfmlib_intel_snbep_unc_ubo.c \
	  pfmlib_intel_snbep_unc_r2pcie.c \
	  pfmlib_intel_snbep_unc_r3qpi.c \
	  pfmlib_intel_ivbep_unc_cbo.c \
	  pfmlib_intel_ivbep_unc_ha.c \
	  pfmlib_intel_ivbep_unc_imc.c \
	  pfmlib_intel_ivbep_unc_pcu.c \
	  pfmlib_intel_ivbep_unc_qpi.c \
	  pfmlib_intel_ivbep_unc_ubo.c \
	  pfmlib_intel_ivbep_unc_r2pcie.c \
	  pfmlib_intel_ivbep_unc_r3qpi.c \
	  pfmlib_intel_ivbep_unc_irp.c \
	  pfmlib_intel_hswep_unc_cbo.c \
	  pfmlib_intel_hswep_unc_ha.c \
	  pfmlib_intel_hswep_unc_imc.c \
	  pfmlib_intel_hswep_unc_pcu.c \
	  pfmlib_intel_hswep_unc_qpi.c \
	  pfmlib_intel_hswep_unc_ubo.c \
	  pfmlib_intel_hswep_unc_r2pcie.c \
	  pfmlib_intel_hswep_unc_r3qpi.c \
	  pfmlib_intel_hswep_unc_irp.c \
	  pfmlib_intel_hswep_unc_sbo.c \
	  pfmlib_intel_bdx_unc_cbo.c \
	  pfmlib_intel_bdx_unc_ubo.c \
	  pfmlib_intel_bdx_unc_sbo.c \
	  pfmlib_intel_bdx_unc_ha.c \
	  pfmlib_intel_bdx_unc_imc.c \
	  pfmlib_intel_bdx_unc_irp.c \
	  pfmlib_intel_bdx_unc_pcu.c \
	  pfmlib_intel_bdx_unc_qpi.c \
	  pfmlib_intel_bdx_unc_r2pcie.c \
	  pfmlib_intel_bdx_unc_r3qpi.c \
	  pfmlib_intel_skx_unc_cha.c \
	  pfmlib_intel_skx_unc_iio.c \
	  pfmlib_intel_skx_unc_imc.c \
	  pfmlib_intel_skx_unc_irp.c \
	  pfmlib_intel_skx_unc_m2m.c \
	  pfmlib_intel_skx_unc_m3upi.c \
	  pfmlib_intel_skx_unc_pcu.c \
	  pfmlib_intel_skx_unc_ubo.c \
	  pfmlib_intel_skx_unc_upi.c \
	  pfmlib_intel_knc.c \
	  pfmlib_intel_slm.c \
	  pfmlib_intel_tmt.c \
	  pfmlib_intel_knl.c \
	  pfmlib_intel_knl_unc_imc.c \
	  pfmlib_intel_knl_unc_edc.c \
	  pfmlib_intel_knl_unc_cha.c \
	  pfmlib_intel_knl_unc_m2pcie.c \
	  pfmlib_intel_glm.c \
	  pfmlib_intel_netburst.c \
	  pfmlib_amd64_k7.c pfmlib_amd64_k8.c pfmlib_amd64_fam10h.c \
	  pfmlib_amd64_fam11h.c pfmlib_amd64_fam12h.c \
	  pfmlib_amd64_fam14h.c pfmlib_amd64_fam15h.c \
	  pfmlib_amd64_fam17h.c pfmlib_amd64_fam16h.c \
	  pfmlib_amd64_fam19h.c pfmlib_amd64_rapl.c \
	  pfmlib_amd64_fam19h_l3.c

CFLAGS += -DCONFIG_PFMLIB_ARCH_X86

ifeq ($(CONFIG_PFMLIB_ARCH_I386),y)
SRCS += pfmlib_intel_coreduo.c pfmlib_intel_p6.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_I386
endif

ifeq ($(CONFIG_PFMLIB_ARCH_X86_64),y)
CFLAGS += -DCONFIG_PFMLIB_ARCH_X86_64
endif

endif

ifeq ($(CONFIG_PFMLIB_ARCH_POWERPC),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_powerpc_perf_event.c
endif

INCARCH = $(INC_POWERPC)
SRCS   += pfmlib_powerpc.c pfmlib_power4.c pfmlib_ppc970.c pfmlib_power5.c \
	pfmlib_power6.c pfmlib_power7.c pfmlib_torrent.c pfmlib_power8.c \
	pfmlib_power9.c pfmlib_powerpc_nest.c pfmlib_power10.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_POWERPC
endif

ifeq ($(CONFIG_PFMLIB_ARCH_S390X),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_s390x_perf_event.c
endif

INCARCH = $(INC_S390X)
SRCS   += pfmlib_s390x_cpumf.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_S390X
endif

ifeq ($(CONFIG_PFMLIB_ARCH_SPARC),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_sparc_perf_event.c
endif

INCARCH = $(INC_SPARC)
SRCS   += pfmlib_sparc.c pfmlib_sparc_ultra12.c pfmlib_sparc_ultra3.c pfmlib_sparc_ultra4.c pfmlib_sparc_niagara.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_SPARC
endif

ifeq ($(CONFIG_PFMLIB_ARCH_ARM),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_arm_perf_event.c
endif

INCARCH = $(INC_ARM)
SRCS   += pfmlib_arm.c pfmlib_arm_armv7_pmuv1.c pfmlib_arm_armv6.c pfmlib_arm_armv8.c pfmlib_tx2_unc_perf_event.c pfmlib_kunpeng_unc_perf_event.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_ARM
endif

ifeq ($(CONFIG_PFMLIB_ARCH_ARM64),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_arm_perf_event.c
endif

INCARCH = $(INC_ARM64)
SRCS   += pfmlib_arm.c pfmlib_arm_armv8.c pfmlib_tx2_unc_perf_event.c pfmlib_kunpeng_unc_perf_event.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_ARM64
endif

ifeq ($(CONFIG_PFMLIB_ARCH_MIPS),y)

ifeq ($(SYS),Linux)
SRCS += pfmlib_mips_perf_event.c
endif

INCARCH = $(INC_MIPS)
SRCS   += pfmlib_mips.c pfmlib_mips_74k.c
CFLAGS += -DCONFIG_PFMLIB_ARCH_MIPS
endif

ifeq ($(CONFIG_PFMLIB_CELL),y)
INCARCH = $(INC_CELL)
#SRCS   += pfmlib_cell.c
CFLAGS += -DCONFIG_PFMLIB_CELL
endif

SRC_CC += $(addprefix $(LIBPFM4_DIR)/lib/,$(SRCS))
vpath %.c $(LIBPFM4_DIR)/lib

CC_OPT += $(CFLAGS)

INC_DIR += $(LIBPFM4_DIR)/include $(LIBPFM4_DIR)/lib/events
vpath %.h $(INC_DIR)

LIBS += base libm libc
