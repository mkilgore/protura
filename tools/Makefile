
disk.img: ./tools/gendisk ./disk
	@echo " GENDISK disk.img"
	$(Q)./tools/gendisk ./disk ./disk.img

./tools/gendisk: ./tools/gendisk.c
	@echo " CCLD    tools/gendisk"
	$(Q)gcc -Wall -g -I./include -I./arch/$(ARCH)/include -o $@ $< 

CLEAN_LIST += ./disk.img ./tools/gendisk

PHONY += gendisk
gendisk: disk.img

