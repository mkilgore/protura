
COREUTILS_CFLAGS := -Wall -std=gnu99 -I./disk/utils/coreutils/include

COREUTILS_PROGS = \
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

DISK_COREUTILS_PROGS := $(patsubst %,$(DISK_BINDIR)/%,$(COREUTILS_PROGS))
DISK_PROGS += $(DISK_COREUTILS_PROGS)

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
	$$(Q)$$(CC) -MM -MP -MF $$@ $$(COREUTILS_CFLAGS) $$< -MT disk/utils/coreutils/$$*.o -MT $$@
endef

$(eval $(call def_util_dep_rule,./disk/utils/coreutils/common))

define def_unil
dir := ./disk/utils/coreutils/$(1)

objs-y :=
common-objs-y :=

include $$(dir)/Makefile

common-obj-list := $$(patsubst %,./disk/utils/coreutils/common/%,$$(common-objs-y))
obj-list := $$(patsubst %,$$(dir)/%,$$(objs-y))

$$(eval $$(call def_util_build_rule,$$(DISK_BINDIR)/$(1),$$(obj-list) $$(common-obj-list), ))

$$(eval $$(call def_util_dep_rule,./disk/utils/coreutils/$(1)))

UTIL_DISK_DEP := $$(obj-list:.o=.d) $$(common-obj-list:.o=.d)

.SECONDARY: $$(obj-list) $$(common-obj-list)
DISK_DEP += $$(foreach file,$$(UTIL_DISK_DEP),$$(dir $$(file)).$$(notdir $$(file)))
DISK_CLEAN_EXTRA += $$(obj-list) $$(common-obj-list)
endef

disk/utils/coreutils/.%.d: disk/utils/coreutils/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(COREUTILS_CFLAGS) $< -MT disk/utils/coreutils/$*.o -MT $@

disk/utils/coreutils/%.o: disk/utils/coreutils/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(COREUTILS_CFLAGS) -c $< -o $@

$(foreach util,$(COREUTILS_PROGS),$(eval $(call def_unil,$(util))))

