
# This target is for creating a multiboot compliant kernel
BOOT_TARGETS += multiboot
LDFLAGS_multiboot := -T $(srctree)/link_multiboot.ld
OBJS_multiboot := $(objtree)/boot_multiboot.o

qemu-test-multiboot: $(output_dir)/imgs/$(EXE)_$(ARCH)_multiboot
	@echo " Testing multiboot with QEMU."
	$(Q)qemu-system-i386 -d int,cpu_reset -s -S -kernel $(output_dir)/imgs/$(EXE)_$(ARCH)_multiboot


