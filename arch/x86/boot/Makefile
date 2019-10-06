
# This target is for creating a multiboot compliant kernel
BOOT_TARGETS += multiboot
LDFLAGS_multiboot := -T $(tree)/link_multiboot.ld -Wl,--build-id=none
OBJS_multiboot := $(tree)/boot_multiboot.o $(tree)/boot_multiboot_pagedir.o
EXTRAS_multiboot := $(tree)/link_multiboot.ld

$(tree)/boot_multiboot_pagedir.S: $(tree)/gen_initial_pagedir.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

CLEAN_LIST += $(tree)/boot_multiboot_pagedir.S

build-iso: $(imgs_pfix)multiboot
	mkdir -p ./imgs/isodir
	mkdir -p ./imgs/isodir/boot
	cp $(imgs_pfix)multiboot ./imgs/isodir/boot
	mkdir -p ./imgs/isodir/grub
	cp grub.cfg ./imgs/isodir/grub/grub.cfg
	grub-mkrescue -o ./imgs/protura.iso ./imgs/isodir

