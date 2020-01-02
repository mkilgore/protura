# Program wide settings
EXE       := protura
VERSION   := 0
SUBLEVEL  := 7
PATCH     := 1

ARCH   := x86
BITS   := 32

PROTURA_DIR := $(PWD)
OBJ_DIR := $(PROTURA_DIR)/obj
BIN_DIR := $(PROTURA_DIR)/bin

KERNEL_DIR := $(BIN_DIR)/kernel
IMGS_DIR := $(BIN_DIR)/imgs
LOGS_DIR := $(BIN_DIR)/logs
TEST_RESULTS_DIR := $(BIN_DIR)/test_results

TARGET := i686-protura
TOOLCHAIN_DIR := $(BIN_DIR)/toolchain
DISK_ROOT := $(OBJ_DIR)/disk_root
DISK_MOUNT := $(OBJ_DIR)/disk_mount

SHELL := /bin/bash
export PATH := $(PATH):$(TOOLCHAIN_DIR)/bin

# Compiler settings
CC      := $(TARGET)-gcc
CPP     := $(TARGET)-cpp
LD      := $(TARGET)-ld
AS      := $(TARGET)-gas
PERL    := perl -w -Mdiagnostics
MKDIR   := mkdir
OBJCOPY := $(TARGET)-objcopy

EXTRA := $(shell ./scripts/version_tag.sh)

VERSION_FULL := $(VERSION).$(SUBLEVEL).$(PATCH)$(EXTRA)

$(info Building: $(EXE))
$(info Arch:     $(ARCH))
$(info Version:  $(VERSION_FULL))

CPPFLAGS  = -DPROTURA_VERSION=$(VERSION)              \
            -DPROTURA_SUBLEVEL=$(SUBLEVEL)            \
            -DPROTURA_PATCH=$(PATCH)                  \
            -DPROTURA_EXTRA="$(EXTRA)"                \
            -DPROTURA_VERSION_FULL="$(VERSION_FULL)"  \
            -DPROTURA_ARCH="$(ARCH)"                  \
            -DPROTURA_BITS=$(BITS)                    \
			-D__KERNEL__                              \
            -I'./include' -I'./arch/$(ARCH)/include'

CFLAGS  := -Wall -O2 -std=gnu99 -ffreestanding \
           -fno-strict-aliasing -nostdlib -fno-builtin -nostdinc

LDFLAGS := -nostdlib -O2 -ffreestanding -lgcc -static-libgcc
ASFLAGS := -DASM -Wall -ffreestanding -nostdlib

# Show all commands executed by the Makefile
# V := y

# Tree is the directory currently being processed
# This changes when decending into directories
tree := .

# Define 'conf' if you want to use a conf file other then
# $(tree)/protura.conf
ifdef conf
	CONFIG_FILE := $(conf)
else
	CONFIG_FILE := $(tree)/protura.conf
	conf := $(CONFIG_FILE)
endif

EXE_OBJ := $(OBJ_DIR)/$(EXE).o

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

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),clean-configure)
ifneq ($(wildcard $(CONFIG_FILE)),$(CONFIG_FILE))
$(error Configuration file $(tree)/protura.conf does not exist. Please create this file before running make or specify different config file via conf variable on commandline)
endif
endif
endif

# We don't include config.mk if we're just going to delete it anyway
ifneq ($(MAKECMDGOALS),clean-configure)
ifneq ($(MAKECMDGOALS),clean-full)
_tmp := $(shell mkdir -p $(tree)/include/protura/config)
# Note - Including a file that doesn't exist provokes make to check for a rule
# This line actually runs the $(tree)/config.mk rule and generates config.mk
# before including it and continuing.
-include $(tree)/config.mk
endif
endif

ifeq ($(CONFIG_KERNEL_DEBUG_SYMBOLS),y)
CPPFLAGS += -DPROTURA_DEBUG
CFLAGS +=  -ggdb -gdwarf-2
ASFLAGS += -ggdb -gdwarf-2
LDFLAGS +=
endif

ifeq ($(CONFIG_FRAME_POINTER),y)
CFLAGS += -fno-omit-frame-pointer
endif

# This includes everything in the 'include' folder of the $(tree)
# This is so that the code can reference generated include files
CPPFLAGS += -I'$(tree)/include/'

make_name = $(subst /,_,$(basename $(tree)/$1))

define dir_rule
$(1): | $(2)
	@echo " MKDIR   $$@"
	$$(Q)$$(MKDIR) $$@
endef

# Define rules for all the basic directories, and all the directories in the
# resulting disk structure
$(eval $(call dir_rule,$(OBJ_DIR),))
$(eval $(call dir_rule,$(BIN_DIR),))

$(eval $(call dir_rule,$(KERNEL_DIR),$(BIN_DIR)))
$(eval $(call dir_rule,$(IMGS_DIR),$(BIN_DIR)))
$(eval $(call dir_rule,$(LOGS_DIR),$(BIN_DIR)))
$(eval $(call dir_rule,$(TEST_RESULTS_DIR),$(BIN_DIR)))
$(eval $(call dir_rule,$(BIN_DIR)/toolchain,$(BIN_DIR)))

$(eval $(call dir_rule,$(DISK_MOUNT),$(OBJ_DIR)))
$(eval $(call dir_rule,$(DISK_ROOT),$(OBJ_DIR)))
$(eval $(call dir_rule,$(DISK_ROOT)/bin,$(DISK_ROOT)))
$(eval $(call dir_rule,$(DISK_ROOT)/usr,$(DISK_ROOT)))
$(eval $(call dir_rule,$(DISK_ROOT)/usr/$(TARGET),$(DISK_ROOT)/usr))
$(eval $(call dir_rule,$(DISK_ROOT)/usr/$(TARGET)/include,$(DISK_ROOT)/usr/$(TARGET)))
$(eval $(call dir_rule,$(DISK_ROOT)/usr/$(TARGET)/lib,$(DISK_ROOT)/usr/$(TARGET)))

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
tree := $$(tree)/$(1)

pfix := $$(subst /,_,$$(tree))_
subdir-y :=
objs-y :=
clean-list-y :=

_tmp := $$(shell mkdir -p $$(tree))
include $$(tree)/Makefile

REAL_OBJS_y += $$(patsubst %,$$(tree)/%,$$(objs-y))
CLEAN_LIST += $$(patsubst %,$$(tree)/%,$$(clean-list-y))

$$(foreach subdir,$$(subdir-y),$$(eval $$(call subdir_inc,$$(subdir))))

tree := $$(patsubst %/$(1),%,$$(tree))
pfix := $$(subst /,_,$$(tree))_
endef


# Include the base directories for source files - That is, the generic 'src'
# directory as well as the 'arch/$(ARCH)' directory
$(eval $(call subdir_inc,src))
$(eval $(call subdir_inc,arch/$(ARCH)))

# This is a list of targets to create bootable images from
# Ex. For x86, there is a multiboot compliant image
BOOT_TARGETS :=

imgs_pfix := $(IMGS_DIR)/$(EXE)_$(ARCH)_

$(eval $(call subdir_inc,arch/$(ARCH)/boot))

define compile_file

_tmp := $$(subst /,_,$$(basename $(1)))_y
ifdef $$(_tmp)

CLEAN_LIST += $$($$(_tmp))

$(1): $$($(_tmp))
	@echo " LD      $$@"
	$$(Q)$$(LD) -r -o $$@ $$($$(_tmp))

endif

endef

REAL_BOOT_TARGETS :=
DEP_LIST :=

define create_boot_target

_expand := $$(KERNEL_DIR)/$$(EXE)_$$(ARCH)_$(1)
REAL_BOOT_TARGETS += $$(_expand)

CLEAN_LIST += $$(OBJS_$(1)) $$(_expand) $$(_expand).full $$(_expand).sym
DEP_LIST += $$(EXTRAS_$(1))

$$(_expand): $$(EXE_OBJ) $$(OBJS_$(1)) $$(EXTRAS_$(1)) | $$(KERNEL_DIR)
	@echo " CCLD    $$@"
	$$(Q)$$(CC) $$(CPPFLAGS) -o $$@ $$(OBJS_$(1)) $$(EXE_OBJ) $$(LDFLAGS) $$(LDFLAGS_$(1)) 
	@echo " COPY    $$@.full"
	$$(Q)cp $$@ $$@.full
	@echo " OBJCOPY $$@.sym"
	$$(Q)$$(OBJCOPY) --only-keep-debug $$@ $$@.sym
	@echo " OBJCOPY $$@"
	$$(Q)$$(OBJCOPY) --strip-unneeded $$@

endef

$(foreach file,$(REAL_OBJS_y),$(eval $(call compile_file,$(file))))

DEP_LIST += $(REAL_OBJS_y)
DEP_LIST := $(foreach file,$(DEP_LIST),$(dir $(file)).$(notdir $(file)))
DEP_LIST := $(DEP_LIST:.o=.d)
DEP_LIST := $(DEP_LIST:.ld=.d)

ifneq (,$(filter $(MAKECMDGOALS),kernel disk check))
-include $(DEP_LIST)
endif

CLEAN_LIST += $(DEP_LIST)

$(foreach btarget,$(BOOT_TARGETS),$(eval $(call create_boot_target,$(btarget))))

# Actual default entry
real-all:
	@echo "Please run make with one of 'configure', 'kernel', 'toolchain', or 'disk'"
	@echo "See README.md for more information"

PHONY += kernel
kernel: configure $(REAL_BOOT_TARGETS) $(EXTRA_TARGETS)

PHONY += configure
configure: $(tree)/config.mk $(tree)/include/protura/config/autoconf.h

PHONY += clean-configure
clean-configure:
	@echo " RM      $(tree)/config.mk"
	$(Q)$(RM) -fr $(tree)/config.mk
	@echo " RM      $(tree)/include/protura/config/autoconf.h"
	$(Q)$(RM) -fr $(tree)/include/protura/config/autoconf.h

$(tree)/config.mk: $(CONFIG_FILE) $(tree)/scripts/genconfig.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $(tree)/scripts/genconfig.pl make < $< > $@

$(tree)/include/protura/config/autoconf.h: $(CONFIG_FILE) $(tree)/scripts/genconfig.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $(tree)/scripts/genconfig.pl cpp < $< > $@

dist: clean-kernel clean-toolchain clean-configure clean-disk
	$(Q)mkdir -p $(EXE)-$(VERSION_N)
	$(Q)cp -R Makefile README.md config.mk LICENSE ./doc ./include ./src ./test $(EXE)-$(VERSION_N)
	$(Q)tar -cf $(EXE)-$(VERSION_N).tar $(EXE)-$(VERSION_N)
	$(Q)gzip $(EXE)-$(VERSION_N).tar
	$(Q)rm -fr $(EXE)-$(VERSION_N)
	@echo " Created $(EXE)-$(VERSION_N).tar.gz"

PHONY += clean-kernel
clean-kernel:
	$(Q)for file in $(REAL_OBJS_y) $(CLEAN_LIST) $(EXE_OBJ) $(IMGS_DIR); do \
		if [ -e $$file ]; then \
		    echo " RM      $$file"; \
			rm -rf $$file; \
		fi \
	done

PHONY += clean-full
clean-full: clean-toolchain clean-configure clean-disk clean-kernel

full: configure install-kernel-headers toolchain kernel disk

clean:
	@echo " Please use one of 'clean-configure', 'clean-toolchain',"
	@echo " 'clean-kernel', 'clean-disk', or 'clean-full'"

$(EXE_OBJ): $(REAL_OBJS_y)
	@echo " LD      $@"
	$(Q)$(LD) -r $(REAL_OBJS_y) -o $@

$(tree)/%.o: $(tree)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_$(make_name $@)) -c $< -o $@

$(tree)/%.o: $(tree)/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(CPPFLAGS) $(ASFLAGS) $(ASFLAGS_$(make_name $@)) -o $@ -c $<

$(tree)/%.ld: $(tree)/%.ldS
	@echo " CPP     $@"
	$(Q)$(CPP) -P $(CPPFLAGS) $(ASFLAGS) -o $@ -x c $<

$(tree)/.%.d: $(tree)/%.ldS
	@echo " CPPDEP   $@"
	$(Q)$(CPP) -MM -MP -MF $@ $(CPPFLAGS) $(ASFLAGS) $< -MT $(tree)/%.ld -MT $@

$(tree)/.%.d: $(tree)/%.c
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $< -MT $(tree)/$*.o -MT $@

$(tree)/.%.d: $(tree)/%.S
	@echo " CCDEP   $@"
	$(Q)$(CC) -MM -MP -MF $@ $(CPPFLAGS) $< -MT $(tree)/$*.o -MT $@

install-kernel-headers: | $(DISK_ROOT)/usr/$(TARGET)/include $(DISK_ROOT)/usr/$(TARGET)/lib
	@echo " CP      include"
	$(Q)cp -r ./include/* $(DISK_ROOT)/usr/$(TARGET)/include/
	@echo " CP      arch/$(ARCH)/include"
	$(Q)cp -r ./arch/$(ARCH)/include/* $(DISK_ROOT)/usr/$(TARGET)/include/
	@echo " LN      include"
	$(Q)ln -fs ./$(TARGET)/include $(DISK_ROOT)/usr/include
	@echo " LN      lib"
	$(Q)ln -fs ./$(TARGET)/lib $(DISK_ROOT)/usr/lib

clean-kernel-headers:
	@echo " RMDIR   include/protura"
	$(Q)rm -fr $(DISK_ROOT)/usr/include/protura

PHONY += toolchain clean-toolchain
toolchain: | $(DISK_ROOT)/usr $(TOOLCHAIN_DIR)
	$(Q)git submodule update --init --depth 1 --recursive
	$(Q)cd ./toolchain; ./build_toolchain.sh $(TARGET) $(DISK_ROOT) /usr $(TOOLCHAIN_DIR) "$(MAKEFLAGS)"

clean-toolchain:
	@echo " RM      $(DISK_ROOT)"
	$(Q)rm -fr $(DISK_ROOT)
	@echo " RM      $(TOOLCHAIN_DIR)"
	$(Q)rm -fr $(TOOLCHAIN_DIR)

PHONY += rebuild-newlib
rebuild-newlib: | $(DISK_ROOT)/usr $(TOOLCHAIN_DIR)
	$(Q)git submodule update --init --depth 1 --recursive
	$(Q)cd ./toolchain; ./build_newlib.sh $(TARGET) $(DISK_ROOT) /usr $(TOOLCHAIN_DIR) "$(MAKEFLAGS)"

include ./userspace/Makefile

.PHONY: $(PHONY)

