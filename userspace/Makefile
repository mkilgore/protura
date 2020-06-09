
USERSPACE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
USERSPACE_DIR := $(USERSPACE_DIR:/=)

DISK_BINDIR := $(DISK_ROOT)/bin
DISK_USRDIR := $(DISK_ROOT)/usr

DISK_PROGS :=
DISK_CLEAN :=

include $(USERSPACE_DIR)/utils/Makefile
include $(USERSPACE_DIR)/coreutils/Makefile
include $(USERSPACE_DIR)/root/Makefile
include $(USERSPACE_DIR)/extras/Makefile

PHONY += disk clean-disk
disk: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

clean-disk:
	$(Q)for file in $(DISK_CLEAN); do \
		if [ -e $$file ]; then \
		    echo " RM      $$file"; \
			rm -rf $$file; \
		fi \
	done

$(IMGS_DIR)/disk.img: $(REAL_BOOT_TARGETS) $(DISK_PROGS) ./scripts/gen_partdisk.sh $(DISK_ROOT_FILES) $(DISK_EXTRA_PROGS) | $(IMGS_DIR) $(DISK_MOUNT)
	@echo " GENDISK $@"
	$(Q)rm -fr $@
	$(Q)sudo ./scripts/gen_partdisk.sh $@ $(DISK_MOUNT) $(DISK_ROOT) $(ROOT_DIR) $(KERNEL_DIR)

$(IMGS_DIR)/disk2.img: ./scripts/gendisk2.sh | $(IMGS_DIR) $(DISK_MOUNT)
	@echo " GENDISK $@"
	$(Q)rm -fr $@
	$(Q)sudo ./scripts/gendisk2.sh $@ $(DISK_MOUNT)

$(IMGS_DIR)/disk.vdi: $(IMGS_DIR)/disk.img | $(IMGS_DIR)
	@echo " QEMUIMG $@"
	$(Q)qemu-img convert -f raw -O vdi $< $@

$(IMGS_DIR)/disk.vhd: $(IMGS_DIR)/disk.img | $(IMGS_DIR)
	@echo " QEMUIMG $@"
	$(Q)qemu-img convert -f raw -O vpc $< $@

PHONY += disk-other
disk-other: $(IMGS_DIR)/disk.vdi $(IMGS_DIR)/disk.vhd

PHONY += gendisk gendisk2
gendisk: disk.img
gendisk2: disk2.img

PHONY += check
check: check-uapi check-kernel check-ext2

PHONY += check-kernel
check-kernel: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(REAL_BOOT_TARGETS) | $(LOGS_DIR)
	$(Q)./scripts/ci/run_tests.sh $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)

PHONY += check-ext2
check-ext2: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(REAL_BOOT_TARGETS) | $(TEST_RESULTS_DIR)
	$(Q)./scripts/ci/run_ext2_tests.sh  $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img $(TEST_RESULTS_DIR)

PHONY += check-uapi
check-uapi:
	$(Q)./scripts/ci/verify_uapi.sh "$(CC)" "./include/uapi" "./arch/$(ARCH)/include/uapi"

PHONY += debug
debug: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_debug_session.sh $(LOGS_DIR) $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

PHONY += debug-ktest
debug-ktest: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_debug_session.sh $(LOGS_DIR) $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img "ktest.run=true"

PHONY += debug-net
debug-net: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_net_debug_session.sh $(LOGS_DIR) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

PHONY += qemu
qemu: $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img | $(LOGS_DIR)
	$(Q)./scripts/start_session.sh $(LOGS_DIR) $(lastword $(REAL_BOOT_TARGETS)) $(IMGS_DIR)/disk.img $(IMGS_DIR)/disk2.img

ifeq ($(MAKECMDGOALS),disk)
-include $(DISK_DEP)
endif
