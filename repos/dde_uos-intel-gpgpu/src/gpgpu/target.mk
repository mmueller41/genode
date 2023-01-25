TARGET = gpgpu
REQUIRES = x86_64

SRC_CC = main.cc gpgpu_genode.cc stubs.cc test.cc ../virt/rpc.cc ../virt/strategies/rr.cc ../virt/strategies/cfs.cc
LIBS   = base

UOS_INTEL_GPGPU = uos-intel-gpgpu-link-cxx.o
EXT_OBJECTS = $(BUILD_BASE_DIR)/bin/$(UOS_INTEL_GPGPU)

$(TARGET): $(UOS_INTEL_GPGPU) $(SRC_CC) ../virt/strategies/config.h

$(UOS_INTEL_GPGPU):
	$(MSG_BUILD) "Building uos-intel-gpgpu..."
	$(MAKE) -C $(REP_DIR)/src/uos-intel-gpgpu/
	cp $(REP_DIR)/src/uos-intel-gpgpu/build/$(UOS_INTEL_GPGPU) $(BUILD_BASE_DIR)/bin/.

clean_uos-intel-gpgpu:
	$(MAKE) -C $(REP_DIR)/src/uos-intel-gpgpu/ clean

clean: clean_uos-intel-gpgpu
