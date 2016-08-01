
DISK_AFLAGS := -std=gnu99 -Wall -O2 -DASM
DISK_CFLAGS := -std=gnu99 -Wall -O2
DISK_LDFLAGS :=

DISK_BINDIR := ./disk/root/bin
DISK_DIRDEPEND := ./disk/root ./disk/root/bin ./disk/root/usr

DISK_UTIL_SRCS := \
	./disk/utils/echo.c \
	./disk/utils/init.c \
	./disk/utils/seg_fault.c \
	./disk/utils/brk_test.c \
	./disk/utils/arg_test.c \
	./disk/utils/pipe_test.c \
	./disk/utils/dsh.c \
	./disk/utils/signal_test.c \
	./disk/utils/orphan.c \

DISK_UTIL_OBJS := $(DISK_UTIL_SRCS:.c=.o)
DISK_UTIL_EXTRA_OBJS :=

include ./disk/utils/coreutils/Makefile

DISK_UTIL_PROGS := $(patsubst ./disk/utils/%.o,$(DISK_BINDIR)/%,$(DISK_UTIL_OBJS))

UTIL_DISK_DEP := $(DISK_UTIL_OBJS:.o=.d) $(DISK_UTIL_EXTRA_OBJS:.o=.d)
DISK_DEP += $(foreach file,$(UTIL_DISK_DEP),$(dir $(file)).$(notdir $(file)))

disk/%.o: disk/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(DISK_AFLAGS) -c -o ./$@ ./$<

disk/%.o: disk/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(DISK_CFLAGS) -c -o ./$@ ./$<

disk/.%.d: disk/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(DISK_CFLAGS) $< -MT disk/$*.o -MT $@

disk/root/bin/%: disk/utils/%.o $(LIBC_DEPEND) | ./disk/root ./disk/root/bin ./disk/root/usr
	@echo " CCLD    $@"
	$(Q)$(CC) -o ./$@ ./$< $(DISK_LDFLAGS)

