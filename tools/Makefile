
disk.img: ./tools/gendisk
	@echo " GENDISK disk.img"
	$(Q)./tools/gendisk ./disk ./disk.img

./tools/gendisk: ./tools/gendisk.c
	@echo " CCLD    tools/gendisk"
	$(Q)gcc -I./include -I./arch/$(ARCH)/include -o $@ $< 

PHONY += gendisk
gendisk: disk.img

