# Program wide settings
EXE       := protura
VERSION   := 0
SUBLEVEL  := 1
PATCH     := 0
VERSION_N := $(VERSION).$(SUBLEVEL).$(PATCH)

ARCH   := x86
BITS   := 32

PROTURA_DIR := $(PWD)

# TARGET := i686-elf-
TARGET := i686-protura-
TOOLCHAIN_DIR := $(PROTURA_DIR)/toolchain

# Compiler settings
CC      := $(TARGET)gcc
CPP     := $(TARGET)gcc -E
LD      := $(TARGET)ld
AS      := $(TARGET)gas
PERL    := perl -w -Mdiagnostics
MKDIR   := mkdir

CPPFLAGS  = -DPROTURA_VERSION=$(VERSION)              \
            -DPROTURA_SUBLEVEL=$(SUBLEVEL)            \
            -DPROTURA_PATCH=$(PATCH)                  \
            -DPROTURA_VERSION_N="$(VERSION_N)"        \
            -DPROTURA_ARCH="$(ARCH)"                  \
            -DPROTURA_BITS=$(BITS)                    \
			-D__KERNEL__                              \
            -I'./include' -I'./arch/$(ARCH)/include'

CFLAGS  := -Wall -O2 -std=gnu99 -ffreestanding \
           -fno-strict-aliasing -nostdlib -fno-builtin

LDFLAGS := -nostdlib -O2 -ffreestanding -lgcc
ASFLAGS := -DASM -Wall -ffreestanding -nostdlib

# Configuration -- Uncomment lines to enable option
# Or specify on the commandline

# Enable debugging
PROTURA_DEBUG := y

# Show all commands executed by the Makefile
# V := y

# Location of source. Currently, the current directory is the only option
srctree := .

# Define 'output_dir' if you want everything to be built in an outside
# directory
ifdef output_dir
	objtree := $(output_dir)
else
	output_dir := .
	objtree := .
endif

# Define 'conf' if you want to use a conf file other then
# $(objtree)/protura.conf
ifdef conf
	CONFIG_FILE := $(conf)
else
	CONFIG_FILE := $(objtree)/protura.conf
	conf := $(CONFIG_FILE)
endif

EXE_OBJ := $(objtree)/$(EXE).o

# This is our default target - The default is the first target in the file so
# we need to define this fairly high-up.
all: real-all

PHONY += all install clean dist qemu-test real-all

# This variable defines any extra targets that should be build by 'all'
EXTRA_TARGETS :=

# This is the internal list of objects to compile into the file .o
REAL_OBJS_y :=

# Predefine this variable. It contains a list of extra files to clean. Ex.
CLEAN_LIST :=

# Set configuration options
ifdef V
	Q :=
else
	Q := @
endif

ifdef PROTURA_DEBUG
	CPPFLAGS += -DPROTURA_DEBUG
	CFLAGS += -ggdb
	ASFLAGS += -ggdb
	LDFLAGS += -ggdb
endif

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clean-configure)
ifneq ($(wildcard $(CONFIG_FILE)),$(CONFIG_FILE))
$(error Configuration file $(objtree)/protura.conf does not exist. Please create this file before running make or specify different config file via conf variable on commandline)
endif
endif
endif

# We don't include config.mk if we're just going to delete it anyway
ifneq ($(MAKECMDGOALS),clean-configure)
_tmp := $(shell mkdir -p $(objtree)/include/protura/config)
# Note - Including a file that doesn't exist provokes make to check for a rule
# This line actually runs the $(objtree)/config.mk rule and generates config.mk
# before including it and continuing.
-include $(objtree)/config.mk
endif

ifeq ($(CONFIG_FRAME_POINTER),y)
CFLAGS += -fno-omit-frame-pointer
endif

# This includes everything in the 'include' folder of the $(objtree)
# This is so that the code can reference generated include files
CPPFLAGS += -I'$(objtree)/include/'

make_name = $(subst /,_,$(basename $(objtree)/$1))

define add_dep
$(1): $(2)
endef

define create_rule
$(1): $(2)
	@echo " $(3)$$@"
	$(4)
endef

# Traverse into tree
define subdir_inc
objtree := $$(objtree)/$(1)
srctree := $$(srctree)/$(1)

pfix := $$(subst /,_,$$(objtree))_
subdir-y :=
objs-y :=
clean-list-y :=

_tmp := $$(shell mkdir -p $$(objtree))
include $$(srctree)/Makefile

REAL_OBJS_y += $$(patsubst %,$$(objtree)/%,$$(objs-y))
CLEAN_LIST += $$(patsubst %,$$(objtree)/%,$$(clean-list-y))

$$(foreach subdir,$$(subdir-y),$$(eval $$(call subdir_inc,$$(subdir))))

srctree := $$(patsubst %/$(1),%,$$(srctree))
objtree := $$(patsubst %/$(1),%,$$(objtree))
pfix := $$(subst /,_,$$(objtree))_
endef


# Include the base directories for source files - That is, the generic 'src'
# directory as well as the 'arch/$(ARCH)' directory
$(eval $(call subdir_inc,src))
$(eval $(call subdir_inc,arch/$(ARCH)))

# This is a list of targets to create bootable images from
# Ex. For x86, there is a multiboot compliant image, also a full realmode image
# that can boot from a floppy.
BOOT_TARGETS :=

imgs_pfix := $(objtree)/imgs/$(EXE)_$(ARCH)_

$(eval $(call subdir_inc,arch/$(ARCH)/boot))

_tmp := $(shell mkdir -p $(objtree)/imgs)

define compile_file

_tmp := $$(subst /,_,$$(basename $(1)))_y
ifdef $$(_tmp)

CLEAN_LIST += $$($$(_tmp))

$(1): $$($(_tmp))
	@echo " LD      $$@"
	$$(Q)$$(LD) -r -o $$@ $$($$(_tmp))

endif

endef

$(foreach file,$(REAL_OBJS_y),$(eval $(call compile_file,$(file))))

DEP_LIST := $(foreach file,$(REAL_OBJS_y),$(dir $(file)).$(notdir $(file)))
DEP_LIST := $(DEP_LIST:.o=.d)

ifeq ($(MAKECMDGOALS),kernel)
-include $(DEP_LIST)
endif

CLEAN_LIST += $(DEP_LIST)


REAL_BOOT_TARGETS :=

define create_boot_target

_expand := $$(objtree)/imgs/$$(EXE)_$$(ARCH)_$(1)
REAL_BOOT_TARGETS += $$(_expand)

CLEAN_LIST += $$(OBJS_$(1))

$$(_expand): $$(EXE_OBJ) $$(OBJS_$(1))
	@echo " CCLD    $$@"
	$$(Q)$$(CC) $$(LDFLAGS) $$(LDFLAGS_$(1)) -o $$@ $$(OBJS_$(1)) $$< 

endef

$(foreach btarget,$(BOOT_TARGETS),$(eval $(call create_boot_target,$(btarget))))

# Actual default entry
real-all:
	@echo "Please run make with one of 'configure', 'kernel', 'toolchain', or 'disk'"
	@echo "See README.md for more information"

PHONY += kernel
kernel: configure $(REAL_BOOT_TARGETS) $(EXTRA_TARGETS)

PHONY += configure
configure: $(objtree)/config.mk $(objtree)/include/protura/config/autoconf.h

PHONY += clean-configure
clean-configure:
	@echo " RM      $(objtree)/config.mk"
	$(Q)$(RM) -fr $(objtree)/config.mk
	@echo " RM      $(objtree)/include/protura/config/autoconf.h"
	$(Q)$(RM) -fr $(objtree)/include/protura/config/autoconf.h

$(objtree)/config.mk: $(CONFIG_FILE) $(srctree)/scripts/genconfig.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $(srctree)/scripts/genconfig.pl make < $< > $@

$(objtree)/include/protura/config/autoconf.h: $(CONFIG_FILE) $(srctree)/scripts/genconfig.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $(srctree)/scripts/genconfig.pl cpp < $< > $@

dist: clean clean-toolchain clean-configure clean-disk
	$(Q)mkdir -p $(EXE)-$(VERSION_N)
	$(Q)cp -R Makefile README.md config.mk LICENSE ./doc ./include ./src ./test $(EXE)-$(VERSION_N)
	$(Q)tar -cf $(EXE)-$(VERSION_N).tar $(EXE)-$(VERSION_N)
	$(Q)gzip $(EXE)-$(VERSION_N).tar
	$(Q)rm -fr $(EXE)-$(VERSION_N)
	@echo " Created $(EXE)-$(VERSION_N).tar.gz"

clean:
	$(Q)for file in $(REAL_OBJS_y) $(CLEAN_LIST) $(EXE_OBJ) $(objtree)/imgs; do \
		if [ -e $$file ]; then \
		    echo " RM      $$file"; \
			rm -rf $$file; \
		fi \
	done

$(EXE_OBJ): $(REAL_OBJS_y)
	@echo " LD      $@"
	$(Q)$(LD) -r $(REAL_OBJS_y) -o $@

$(objtree)/%.o: $(srctree)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_$(make_name $@)) -c $< -o $@

$(objtree)/%.o: $(srctree)/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(CPPFLAGS) $(ASFLAGS) $(ASFLAGS_$(make_name $@)) -o $@ -c $<

$(objtree)/%.ld: $(srctree)/%.ldS
	@echo " CPP     $@"
	$(Q)$(CPP) -P $(CPPFLAGS) $(ASFLAGS) -o $@ -x c $<

$(objtree)/.%.d: $(srctree)/%.ldS
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $(ASFLAGS) $< -MT $(objtree)/%.o -MT $@

$(objtree)/.%.d: $(srctree)/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $< -MT $(objtree)/$*.o -MT $@

$(objtree)/.%.d: $(srctree)/%.S
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $< -MT $(objtree)/$*.o -MT $@



$(objtree)/%.o: $(objtree)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_$(make_name $@)) -c $< -o $@

$(objtree)/%.o: $(objtree)/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(CPPFLAGS) $(ASFLAGS) $(ASFLAGS_$(make_name $@)) -o $@ -c $<

$(objtree)/%.ld: $(objtree)/%.ldS
	@echo " CPP     $@"
	$(Q)$(CPP) -P $(CPPFLAGS) $(ASFLAGS) -o $@ -x c $<

$(objtree)/.%.d: $(objtree)/%.ldS
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $(ASFLAGS) $< -MT $(objtree)/%.o -MT $@

$(objtree)/.%.d: $(objtree)/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $< -MT $(objtree)/$*.o -MT $@

$(objtree)/.%.d: $(objtree)/%.S
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $< -MT $(objtree)/$*.o -MT $@

install-kernel-headers: | ./disk/root/usr/include
	@echo " CP      include"
	$(Q)cp -r ./include/* ./disk/root/usr/include/
	@echo " CP      arch/$(ARCH)/include"
	$(Q)cp -r ./arch/$(ARCH)/include/* ./disk/root/usr/include/

PHONY += cscope
cscope:
	@echo " Generating cscope for arch $(ARCH)" 
	$(Q)find ./ \
		-path "./arch/*" ! -path "./arch/$(ARCH)/*" -prune -o \
		-path "./scripts/*" -prune -o \
		-name "*.[chsS]" -print \
		> ./cscope.files
	$(Q)cscope -b -q -k

include ./disk/Makefile

.PHONY: $(PHONY)

