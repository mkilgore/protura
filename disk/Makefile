
DISK_AFLAGS := -std=gnu99 -Wall -O2 -DASM -I./include -I./arch/$(ARCH)/include
DISK_CFLAGS := -std=gnu99 -Wall -O2 -I./include -I./arch/$(ARCH)/include
DISK_LDFLAGS :=

DISK_UTIL_SRCS := \
	./disk/utils/echo.c \
	./disk/utils/init.c \
	./disk/utils/seg_fault.c \
	./disk/utils/brk_test.c \
	./disk/utils/ls.c \
	./disk/utils/arg_test.c \
	./disk/utils/pipe_test.c

DISK_UTIL_OBJS := $(DISK_UTIL_SRCS:.c=.o)
DISK_UTIL_PROGS := $(patsubst ./disk/utils/%.o,./disk/root/bin/%,$(DISK_UTIL_OBJS))

define dir_rule
$(1): | $(2)
	@echo " MKDIR   $$@"
	$$(Q)$$(MKDIR) $$@
endef

$(eval $(call dir_rule,disk/root,))
$(eval $(call dir_rule,disk/root/bin,disk/root))
$(eval $(call dir_rule,disk/root/usr,disk/root))
$(eval $(call dir_rule,disk/root/usr/i686-protura,disk/root/usr))
$(eval $(call dir_rule,disk/root/usr/i686-protura/include,disk/root/usr/i686-protura))

disk/%.o: disk/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(DISK_AFLAGS) -c -o ./$@ ./$<

disk/%.o: disk/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(DISK_CFLAGS) -c -o ./$@ ./$<

disk/root/bin/%: disk/utils/%.o $(LIBC_DEPEND) | ./disk/root ./disk/root/bin ./disk/root/usr
	@echo " LD      $@"
	$(Q)$(CC) -o ./$@ ./$< $(DISK_LDFLAGS)

PHONY += disk clean-disk
disk: disk.img

clean-disk:
	$(Q)for file in $(DISK_PROGS) $(DISK_UTIL_OBJS) ./disk.img; do \
		if [ -e $$file ]; then \
		    echo " RM      $$file"; \
			rm -rf $$file; \
		fi \
	done

PHONY += toolchain clean-toolchain
toolchain: | ./disk/root ./disk/root/usr
	$(Q)git submodule update --init --depth 1
	$(Q)cd ./disk/toolchain; git submodule update --init --depth 1
	$(Q)cd ./disk/toolchain; ./build_toolchain.sh $(PROTURA_DIR)/disk/root /usr $(TOOLCHAIN_DIR)

clean-toolchain:
	@echo " RM      ./disk/root"
	$(Q)rm -fr ./disk/root
	@echo " RM      ./toolchain/*"
	$(Q)rm -fr ./toolchain/*

PHONY += rebuild-newlib
rebuild-newlib:
	$(Q)cd ./disk/toolchain; ./build_newlib.sh $(PROTURA_DIR)/disk/root /usr $(TOOLCHAIN_DIR)

.SECONDARY: $(DISK_UTIL_OBJS)

DISK_PROGS += $(DISK_UTIL_PROGS)

disk.img: $(REAL_BOOT_TARGETS) $(DISK_PROGS)
	@echo " GENDISK $@"
	$(Q)sudo ./scripts/gendisk.sh

gendisk: disk.img

