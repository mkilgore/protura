
LIBC_OBJS := \
	./disk/lib/c/crt0.o \
	./disk/lib/c/string.o \


ifdef no_defined
disk/lib/c/%.o: disk/libc/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(DISK_AFLAGS) -c -o ./$@ ./$<

disk/libc/%.o: disk/libc/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(DISK_CFLAGS) -c -o ./$@ ./$<

endif

$(DISK_LIBC): $(LIBC_OBJS)
	@echo " LD      $@"
	$(Q)$(LD) -r -o ./$@ $(LIBC_OBJS)

CLEAN_LIST += $(LIBC_OBJS)

.SECONDARY: $(LIBC_OBJS)

