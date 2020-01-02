
COREUTILS_BASE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
COREUTILS_BASE_DIR := $(COREUTILS_BASE_DIR:/=)

COREUTILS_CFLAGS := -Wall -std=gnu99 -I$(COREUTILS_BASE_DIR)/include -D_GNU_SOURCE

COREUTILS_PROG_LIST = \
	cat \
	false \
	files \
	getty \
	head \
	hexdump \
	ifconfig \
	kill \
	link \
	ln \
	lnum \
	ls \
	mem \
	mkdir \
	mkfifo \
	mknod \
	more \
	mount \
	mv \
	ping \
	ps \
	rm \
	rmdir \
	route \
	seq \
	sh \
	sort \
	stat \
	sync \
	tee \
	touch \
	true \
	tty \
	umount \
	uniq \
	unlink \
	wc \
	stty \
	db_display \
	userdel \
	usermod \
	init \
	login \
	id \
	env \
	truncate \
	uname \
	klogd \
	grep \
	reboot \
	run_test \

COREUTILS_PROGS := $(patsubst %,$(DISK_BINDIR)/%,$(COREUTILS_PROG_LIST))

COREUTILS_DEP :=
COREUTILS_OBJS :=

# Arg 1 is the build target name
# Arg 2 is the build target dependencies
# Arg 3 is any extra flags to be given to the compiler
define def_util_build_rule
$(1): $(2) | $$(DISK_BINDIR)
	@echo " CCLD    $$@"
	$$(Q)$$(CC) $$(COREUTILS_CFLAGS) $(3) $(2) -o $(1)
	@echo " OBJCOPY $$@"
	$$(Q)$$(OBJCOPY) --strip-unneeded $$@
endef

define def_util_dep_rule
$(1)/.%.d: $(1)/%.c
	@echo " CCDEP   $$@"
	$$(Q)$$(CC) -MM -MP -MF $$@ $$(COREUTILS_CFLAGS) $$< -MT $(COREUTILS_BASE_DIR)/$$*.o -MT $$@
endef

$(eval $(call def_util_dep_rule,$(COREUTILS_BASE_DIR)/common))

define def_unil
dir := $(COREUTILS_BASE_DIR)/$(1)

objs-y :=
common-objs-y :=

include $$(dir)/Makefile

common-obj-list := $$(patsubst %,$(COREUTILS_BASE_DIR)/common/%,$$(common-objs-y))
obj-list := $$(patsubst %,$$(dir)/%,$$(objs-y))

$$(eval $$(call def_util_build_rule,$$(DISK_BINDIR)/$(1),$$(obj-list) $$(common-obj-list), ))

$$(eval $$(call def_util_dep_rule,$(COREUTILS_BASE_DIR)/$(1)))

COREUTILS_DEP += $$(obj-list:.o=.d) $$(common-obj-list:.o=.d)
COREUTILS_OBJS += $$(obj-list) $$(common-obj-list)
endef

$(foreach util,$(COREUTILS_PROG_LIST),$(eval $(call def_unil,$(util))))

COREUTILS_DEP += $(foreach file,$(COREUTILS_DEP),$(dir $(file)).$(notdir $(file)))

.SECONDARY: $(COREUTILS_OBJS)

DISK_PROGS += $(COREUTILS_PROGS)
DISK_DEP += $(COREUTILS_DEP)
DISK_CLEAN += $(COREUTILS_OBJS) $(COREUTILS_PROGS) $(COREUTILS_DEP)

$(COREUTILS_BASE_DIR)/.%.d: $(COREUTILS_BASE_DIR)/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(COREUTILS_CFLAGS) $< -MT $(COREUTILS_BASE_DIR)/$*.o -MT $@

$(COREUTILS_BASE_DIR)/%.o: $(COREUTILS_BASE_DIR)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(COREUTILS_CFLAGS) -c $< -o $@

