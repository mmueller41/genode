MIRROR_FROM_REP_DIR := lib/mk/mxtasking.mk lib/import/import-mxtasking.mk

content: src/lib/mxtasking LICENSE $(MIRROR_FROM_REP_DIR)

PORT_DIR := $(call port_dir,$(REP_DIR)/ports/mxtasking)

src/lib/mxtasking:
	mkdir -p $@
	cp -r $(PORT_DIR)/src/lib/mxtasking/* $@
	rm -rf $@/.git
	echo "LIBS = mxtasking" > $@/target.mk

$(MIRROR_FROM_REP_DIR):
	$(mirror_from_rep_dir)

LICENSE:
	echo "mxtasking license, see src/lib/mxtasking/LICENSE" > $@

