
LINK_TEXT := 0x00800000

DISK_LIBC := ./disk/lib/libc.o

include ./disk/lib/c/Makefile

DISK_AFLAGS := -std=gnu99 -Wall -O2 -I./disk/usr/include -I./include -I./arch/$(ARCH)/include -ffreestanding -DASM -nostdlib -Wl,-Ttext=$(LINK_TEXT)
DISK_CFLAGS := -std=gnu99 -Wall -O2 -I./disk/usr/include -I./include -I./arch/$(ARCH)/include -fno-asynchronous-unwind-tables -ffreestanding -nostdlib -Wl,-Ttext=$(LINK_TEXT)
DISK_LDFLAGS := -e _start -Ttext $(LINK_TEXT) $(DISK_LIBC)

DISK_SRCS := \
	./disk/src/echo.c \
	./disk/src/init.c \
	./disk/src/seg_fault.c \
	./disk/src/brk_test.c \
	./disk/src/ls.c \
	./disk/src/arg_test.c \

DISK_OBJS := $(DISK_SRCS:.c=.o)
DISK_PROGS := $(patsubst ./disk/src/%.o,./disk/bin/%,$(DISK_OBJS))

disk/%.o: disk/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(DISK_AFLAGS) -c -o ./$@ ./$<

disk/%.o: disk/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(DISK_CFLAGS) -c -o ./$@ ./$<

disk/bin/%: disk/src/%.o $(DISK_LIBC)
	@echo " LD      $@"
	$(Q)$(LD) $(DISK_LDFLAGS) -o ./$@ ./$<

CLEAN_LIST += $(DISK_PROGS) $(DISK_OBJS) $(DISK_LIBC)

.SECONDARY: $(DISK_OBJS) $(DISK_LIBC)

