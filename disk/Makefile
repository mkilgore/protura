
DISK_SRCS := \
	./disk/test_prog.c \
	./disk/exec_prog.c \
	./disk/echo.c \
	./disk/init.c \
	./disk/seg_fault.c \
	./disk/brk_test.c \
	./disk/ls.c \

DISK_OBJS := $(DISK_SRCS:.c=.o)
DISK_PROGS := $(DISK_OBJS:.o=)

disk/%.o: disk/%.c
	@echo " CC      $@"
	$(Q)$(CC) -O2 -I./include -I./arch/$(ARCH)/include -fno-asynchronous-unwind-tables -ffreestanding -nostdlib -Wl,-emain -Wl,-Ttext=0x00800000 -o ./$@ ./$<

disk/%: disk/%.o
	@echo " LD      $@"
	$(Q)$(LD) -e main -o ./$@ ./$< -Ttext 0x00800000

CLEAN_LIST += $(DISK_PROGS) $(DISK_OBJS)

.SECONDARY: $(DISK_OBJS)

