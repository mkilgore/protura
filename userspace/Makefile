
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

$(IMGS_DIR)/disk.img: $(KERNEL) $(DISK_PROGS) ./scripts/gen_partdisk.sh ./scripts/copy_root.sh ./scripts/grub.cfg $(DISK_ROOT_FILES) $(DISK_EXTRA_PROGS) | $(IMGS_DIR) $(DISK_MOUNT)
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

ifeq ($(MAKECMDGOALS),disk)
-include $(DISK_DEP)
endif
