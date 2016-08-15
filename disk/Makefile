
DISK_DEP :=
DISK_CLEAN_EXTRA :=

include ./disk/utils/Makefile

define dir_rule
$(1): | $(2)
	@echo " MKDIR   $$@"
	$$(Q)$$(MKDIR) $$@
endef

$(eval $(call dir_rule,disk/root,))
$(eval $(call dir_rule,disk/root/bin,disk/root))
$(eval $(call dir_rule,disk/root/usr,disk/root))
$(eval $(call dir_rule,disk/root/usr/i686-protura,disk/root/usr))
$(eval $(call dir_rule,disk/root/usr/i686-protura/include,disk/root/usr/i686-protura))

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
	$(Q)git submodule update --init --depth 1
	$(Q)cd ./disk/toolchain; git submodule update --init --depth 1
	$(Q)cd ./disk/toolchain; ./build_toolchain.sh $(PROTURA_DIR)/disk/root /usr $(TOOLCHAIN_DIR)

clean-toolchain:
	@echo " RM      ./disk/root"
	$(Q)rm -fr ./disk/root
	@echo " RM      ./toolchain/*"
	$(Q)rm -fr ./toolchain/*

PHONY += rebuild-newlib
rebuild-newlib:
	$(Q)cd ./disk/toolchain; ./build_newlib.sh $(PROTURA_DIR)/disk/root /usr $(TOOLCHAIN_DIR)

.SECONDARY: $(DISK_UTIL_OBJS)

DISK_PROGS += $(DISK_UTIL_PROGS)

disk.img: $(REAL_BOOT_TARGETS) $(DISK_PROGS) ./scripts/gendisk.sh
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk.sh

disk2.img: ./scripts/gendisk2.sh
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk2.sh

gendisk: disk.img
gendisk2: disk2.img


ifeq ($(MAKECMDGOALS),disk)
-include $(DISK_DEP)
endif

