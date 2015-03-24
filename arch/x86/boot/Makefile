
# This target is for creating a multiboot compliant kernel
BOOT_TARGETS += multiboot
LDFLAGS_multiboot := -T $(objtree)/link_multiboot.ld -g
OBJS_multiboot := $(objtree)/boot_multiboot.o $(objtree)/boot_multiboot_pagedir.o $(objtree)/link_multiboot.ld

$(objtree)/boot_multiboot_pagedir.S: $(srctree)/gen_initial_pagedir.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

CLEAN_LIST += $(objtree)/boot_multiboot_pagedir.S

qemu-test-multiboot: $(imgs_pfix)multiboot
	@echo " Testing multiboot with QEMU."
	$(Q)qemu-system-i386 -curses  -s -S -d int,cpu_reset -kernel $(imgs_pfix)multiboot

build-iso: $(imgs_pfix)multiboot
	mkdir -p ./imgs/isodir
	mkdir -p ./imgs/isodir/boot
	cp $(imgs_pfix)multiboot ./imgs/isodir/boot
	mkdir -p ./imgs/isodir/grub
	cp grub.cfg ./imgs/isodir/grub/grub.cfg
	grub2-mkrescue -o ./imgs/protura.iso ./imgs/isodir

