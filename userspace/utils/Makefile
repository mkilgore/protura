
UTILS_BASE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
UTILS_BASE_DIR := $(UTILS_BASE_DIR:/=)

UTILS_AFLAGS := -std=gnu99 -Wall -O2 -DASM
UTILS_CFLAGS := -std=gnu99 -Wall -O2
UTILS_LDFLAGS :=

UTILS_SRCS := \
	$(UTILS_BASE_DIR)/echo.c \
	$(UTILS_BASE_DIR)/seg_fault.c \
	$(UTILS_BASE_DIR)/brk_test.c \
	$(UTILS_BASE_DIR)/arg_test.c \
	$(UTILS_BASE_DIR)/pipe_test.c \
	$(UTILS_BASE_DIR)/dsh.c \
	$(UTILS_BASE_DIR)/signal_test.c \
	$(UTILS_BASE_DIR)/orphan.c \
	$(UTILS_BASE_DIR)/edit.c \
	$(UTILS_BASE_DIR)/time.c \
	$(UTILS_BASE_DIR)/poll_test.c \
	$(UTILS_BASE_DIR)/fifo_test.c \
	$(UTILS_BASE_DIR)/waitpid_test.c \
	$(UTILS_BASE_DIR)/socket_test.c \
	$(UTILS_BASE_DIR)/color_test.c \
	$(UTILS_BASE_DIR)/tcp_test.c \

UTILS_OBJS := $(UTILS_SRCS:.c=.o)
UTILS_EXTRA_OBJS :=

UTILS_PROGS := $(foreach file,$(UTILS_OBJS),$(notdir $(file)))
UTILS_PROGS := $(patsubst %.o,$(DISK_BINDIR)/%,$(UTILS_PROGS))

UTIL_DISK_DEP := $(UTILS_OBJS:.o=.d) $(UTILS_EXTRA_OBJS:.o=.d)
UTIL_DISK_DEP := $(foreach file,$(UTIL_DISK_DEP),$(dir $(file)).$(notdir $(file)))

.SECONDARY: $(UTILS_OBJS)

DISK_PROGS += $(UTILS_PROGS)
DISK_CLEAN += $(UTIL_DISK_DEP) $(UTILS_PROGS) $(UTILS_OBJS) $(UTILS_EXTRA_OBJS)
DISK_DEP += $(UTIL_DISK_DEP)

$(UTILS_BASE_DIR)/%.o: $(UTILS_BASE_DIR)/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(UTILS_AFLAGS) -c -o $@ $<

$(UTILS_BASE_DIR)/%.o: $(UTILS_BASE_DIR)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(UTILS_CFLAGS) -c -o $@ $<

$(UTILS_BASE_DIR)/.%.d: $(UTILS_BASE_DIR)/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(UTILS_CFLAGS) $< -MT $(UTILS_BASE_DIR)/$*.o -MT $@

$(DISK_BINDIR)/%: $(UTILS_BASE_DIR)/%.o $(LIBC_DEPEND) | $(DISK_BINDIR)
	@echo " CCLD    $@"
	$(Q)$(CC) -o $@ $< $(DISK_LDFLAGS)
	@echo " OBJCOPY $@"
	$(Q)$(OBJCOPY) --strip-unneeded $@

