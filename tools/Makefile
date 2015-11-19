
disk.img: $(DISK_PROGS) ./disk
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk.sh

CLEAN_LIST += ./disk.img 

gendisk: disk.img

