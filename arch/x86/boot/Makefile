
# This target is for creating a multiboot compliant kernel
BOOT_TARGETS += multiboot
LDFLAGS_multiboot := -T $(objtree)/link_multiboot.ld
OBJS_multiboot := $(objtree)/boot_multiboot.o $(objtree)/link_multiboot.ld

qemu-test-multiboot: $(imgs_pfix)multiboot
	@echo " Testing multiboot with QEMU."
	$(Q)qemu-system-i386 -d int,cpu_reset -s -S -kernel $(imgs_pfix)multiboot

