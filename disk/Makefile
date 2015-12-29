
#LINK_TEXT := 0x00800000

#DISK_AFLAGS := -std=gnu99 -Wall -O2 -I./disk/root/usr/include -I./include -I./arch/$(ARCH)/include -ffreestanding -DASM -nostdlib -Wl,-Ttext=$(LINK_TEXT)
#DISK_CFLAGS := -std=gnu99 -Wall -O2 -I./disk/root/usr/include -I./include -I./arch/$(ARCH)/include -fno-asynchronous-unwind-tables -ffreestanding -nostdlib -Wl,-Ttext=$(LINK_TEXT)
#DISK_LDFLAGS := -e _start -Ttext $(LINK_TEXT)

DISK_AFLAGS := -std=gnu99 -Wall -O2 -DASM
DISK_CFLAGS := -std=gnu99 -Wall -O2
DISK_LDFLAGS :=

#CLEAN_LIST += ./disk/lib/newlib-build ./disk/usr/newlib

#./disk/root/usr/lib/libc.a: ./disk/libc/protura-newlib
#	$(Q)./disk/libc/build_newlib.sh $(PROTURA_DIR)/disk/libc $(PROTURA_DIR)/disk/root

#LIBC_DEPEND := ./disk/root/usr/lib/libc.a

#DISK_LDFLAGS += -static ./disk/root/usr/lib/libc.a \
				./disk/root/usr/lib/libm.a

DISK_UTIL_SRCS := \
	./disk/utils/echo.c \
	./disk/utils/init.c \
	./disk/utils/seg_fault.c \
	./disk/utils/brk_test.c \
	./disk/utils/ls.c \
	./disk/utils/arg_test.c \

DISK_UTIL_OBJS := $(DISK_UTIL_SRCS:.c=.o)
DISK_UTIL_PROGS := $(patsubst ./disk/utils/%.o,./disk/root/bin/%,$(DISK_UTIL_OBJS))

disk/root:
	@echo " MKDIR   $@"
	$(Q)$(MKDIR) $@

disk/root/bin: | ./disk/root
	@echo " MKDIR   $@"
	$(Q)$(MKDIR) $@

disk/root/usr: | ./disk/root
	@echo " MKDIR   $@"
	$(Q)$(MKDIR) $@

disk/root/usr/include: | ./disk/root/usr
	@echo " MKDIR   $@"
	$(Q)$(MKDIR) $@

disk/%.o: disk/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(DISK_AFLAGS) -c -o ./$@ ./$<

disk/%.o: disk/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(DISK_CFLAGS) -c -o ./$@ ./$<

disk/root/bin/%: disk/utils/%.o $(LIBC_DEPEND) | ./disk/root ./disk/root/bin ./disk/root/usr
	@echo " LD      $@"
	$(Q)$(LD) -o ./$@ ./$< $(DISK_LDFLAGS)

PHONY += disk clean-disk
disk: disk.img

clean-disk:
	$(Q)for file in $(DISK_PROGS) $(DISK_UTIL_OBJS); do \
		if [ -e $$file ]; then \
		    echo " RM      $$file"; \
			rm -rf $$file; \
		fi \
	done

PHONY += toolchain clean-toolchain
toolchain: | ./disk/root ./disk/root/usr
	@cd ./disk/toolchain && git submodule foreach git pull
	$(Q)./disk/toolchain/build_toolchain.sh ./disk/root /usr $(TOOLCHAIN_DIR)

clean-toolchain:
	@echo " RM      ./disk/root"
	$(Q)rm -fr ./disk/root
	@echo " RM      ./toolchain/*"
	$(Q)rm -fr ./toolchain/*

PHONY += rebuild-newlib
rebuild-newlib:
	$(Q)./disk/toolchain/build_newlib.sh ./disk/root /usr $(TOOLCHAIN_DIR)

CLEAN_LIST += $(DISK_UTIL_PROGS) $(DISK_UTIL_OBJS)

.SECONDARY: $(DISK_UTIL_OBJS)

DISK_PROGS += $(DISK_UTIL_PROGS)


disk.img: $(DISK_PROGS)
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk.sh

CLEAN_LIST += ./disk.img

gendisk: disk.img

