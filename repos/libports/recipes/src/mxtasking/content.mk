content: src/lib/mxtasking lib/mk/mxtasking.mk LICENSE

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mxtasking)

src/lib/mxtasking:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/mxtasking/* $@
	echo "LIBS = mxtasking" > $@/target.mk

lib/mk/mxtasking.mk:
	$(mirror_from_rep_dir)

LICENSE:
	echo "mxtsaking license, see src/lib/mxtasking/LICENSE" > $@

