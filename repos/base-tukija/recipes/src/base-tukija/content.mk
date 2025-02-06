include $(GENODE_DIR)/repos/base/recipes/src/base_content.inc

content: README
README:
	cp $(REP_DIR)/recipes/src/base-tukija/README $@

content: src/kernel/tukija
src/kernel:
	$(mirror_from_rep_dir)

KERNEL_PORT_DIR := $(call port_dir,$(REP_DIR)/ports/tukija)

src/kernel/tukija: src/kernel
	cp -r $(KERNEL_PORT_DIR)/src/kernel/tukija/* $@

content:
	for spec in x86_32 x86_64; do \
	  mv lib/mk/spec/$$spec/ld-tukija.mk lib/mk/spec/$$spec/ld.mk; \
	  done;
	sed -i "s/tukija_timer/timer/" src/timer/tukija/target.mk

