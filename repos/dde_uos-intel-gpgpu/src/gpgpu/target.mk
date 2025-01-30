TARGET = gpgpu
REQUIRES = x86_64

UOS_INTEL_GPGPU = uos-intel-gpgpu-link-cxx.o
GPGPU_DRIVER_PATH = $(REP_DIR)/src/uos-intel-gpgpu
EXT_OBJECTS = $(BUILD_BASE_DIR)/bin/$(UOS_INTEL_GPGPU)

SRC_CC = main.cc gpgpu_genode.cc stubs.cc test.cc ../virt/rpc.cc ../virt/strategies/rr.cc ../virt/strategies/cfs.cc
LIBS   = base
INC_DIR += $(GPGPU_DRIVER_PATH)

$(TARGET): $(UOS_INTEL_GPGPU) $(SRC_CC) ../config.h

$(UOS_INTEL_GPGPU):
	$(MSG_BUILD) "Building uos-intel-gpgpu..."
	$(MAKE) -C $(GPGPU_DRIVER_PATH)
	cp $(GPGPU_DRIVER_PATH)/build/$(UOS_INTEL_GPGPU) $(BUILD_BASE_DIR)/bin/.

clean_uos-intel-gpgpu:
	$(MAKE) -C $(GPGPU_DRIVER_PATH) clean

clean: clean_uos-intel-gpgpu
