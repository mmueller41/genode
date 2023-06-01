MIRROR_FROM_REP_DIR := lib/mk/libpfm4.mk lib/import/import-libpfm4.mk

content: src/lib/libpfm4 COPYING $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/libpfm4)

src/lib/libpfm4:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/libpfm4/* $@
	rm -rf $@/.git
	echo "LIBS = libpfm4" > $@/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	echo "libpfm license, see src/lib/libpfm4/COPYING" > $@