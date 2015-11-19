
#disk.img: $(DISK_PROGS) ./tools/gendisk ./disk
#	@echo " GENDISK disk.img"
#	$(Q)./tools/gendisk ./disk ./disk.img

disk.img: $(DISK_PROGS) ./disk
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk.sh

./tools/gendisk: ./tools/gendisk.c
	@echo " CCLD    tools/gendisk"
	$(Q)gcc -Wall -g -I./include -I./arch/$(ARCH)/include -o $@ $< 

CLEAN_LIST += ./disk.img ./tools/gendisk

PHONY += gendisk
gendisk: disk.img

