
DISK_DEP :=
DISK_CLEAN_EXTRA :=

DISK_ROOT := $(PROTURA_DIR)/disk/root

include ./disk/utils/Makefile
include ./disk/extras/Makefile

define dir_rule
$(1): | $(2)
	@echo " MKDIR   $$@"
	$$(Q)$$(MKDIR) $$@
endef

$(eval $(call dir_rule,disk/root,))
$(eval $(call dir_rule,disk/root/bin,disk/root))
$(eval $(call dir_rule,disk/root/usr,disk/root))
$(eval $(call dir_rule,disk/root/usr/$(TARGET),disk/root/usr))
$(eval $(call dir_rule,disk/root/usr/$(TARGET)/include,disk/root/usr/$(TARGET)))

PHONY += disk clean-disk
disk: disk.img disk2.img

clean-disk:
	$(Q)for file in $(DISK_PROGS) $(DISK_UTIL_OBJS) $(DISK_UTIL_EXTRA_OBJS) $(DISK_DEP) $(DISK_CLEAN_EXTRA) ./disk.img ./disk2.img; do \
		if [ -e $$file ]; then \
		    echo " RM      $$file"; \
			rm -rf $$file; \
		fi \
	done

PHONY += toolchain clean-toolchain
toolchain: | ./disk/root ./disk/root/usr
	$(Q)git submodule update --init --depth 1 --recursive
	$(Q)cd ./toolchain; ./build_toolchain.sh $(TARGET) $(DISK_ROOT) /usr $(TOOLCHAIN_DIR) "$(MAKEFLAGS)"

clean-toolchain:
	@echo " RM      ./disk/root"
	$(Q)rm -fr ./disk/root
	@echo " RM      ./bin/*"
	$(Q)rm -fr ./bin/*

PHONY += rebuild-newlib
rebuild-newlib:
	$(Q)git submodule update --init --depth 1 --recursive
	$(Q)cd ./toolchain; ./build_newlib.sh $(TARGET) $(DISK_ROOT) /usr $(TOOLCHAIN_DIR) "$(MAKEFLAGS)"

.SECONDARY: $(DISK_UTIL_OBJS)

DISK_PROGS += $(DISK_UTIL_PROGS)

DISK_EXTRA_FILES := $(wildcard ./disk/etc/*) $(wildcard ./disk/home/*) $(wildcard ./disk/tests/*)

disk.img: $(REAL_BOOT_TARGETS) $(DISK_PROGS) ./scripts/gen_partdisk.sh $(DISK_EXTRA_FILES) $(wildcard ./disk/extras/*.built)
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gen_partdisk.sh $@

disk2.img: ./scripts/gendisk2.sh
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk2.sh

disk.vdi: disk.img
	@echo " QEMUIMG $@"
	$(Q)qemu-img convert -f raw -O vdi $< $@

disk.vhd: disk.img
	@echo " QEMUIMG $@"
	$(Q)qemu-img convert -f raw -O vpc $< $@

PHONY += disk-other
disk-other: disk.vdi disk.vhd

PHONY += gendisk gendisk2
gendisk: disk.img
gendisk2: disk2.img

PHONY += check
check: check-kernel check-ext2

PHONY += check-kernel
check-kernel: disk.img disk2.img
	$(Q)./scripts/ci/run_tests.sh

PHONY += check-ext2
check-ext2: disk.img disk2.img
	$(Q)./scripts/ci/run_ext2_tests.sh $(TEST)

ifeq ($(MAKECMDGOALS),disk)
-include $(DISK_DEP)
endif

