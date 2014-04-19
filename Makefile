# Program wide settings
EXE       := protura
VERSION   := 0
SUBLEVEL  := 1
PATCH     := 0
VERSION_N := $(VERSION).$(SUBLEVEL).$(PATCH)

ARCH   := x86
BITS   := 32

TARGET := i586-elf-

# Compiler settings
CC      := $(TARGET)gcc
CPP     := $(TARGET)gcc -E
LD      := $(TARGET)ld
AS      := $(TARGET)gas
PERL    := perl

CPPFLAGS  = -DPROTURA_VERSION=$(VERSION)              \
            -DPROTURA_SUBLEVEL=$(SUBLEVEL)            \
            -DPROTURA_PATCH=$(PATCH)                  \
            -DPROTURA_VERSION_N="$(VERSION_N)"        \
            -DPROTURA_ARCH="$(ARCH)"                  \
			-DBITS=$(BITS)                            \
            -I'./include' -I'./arch/$(ARCH)/include'

CFLAGS  := -Wall -O2 -std=gnu99 -ffreestanding        \
           -fno-strict-aliasing -nostdlib

LDFLAGS := -nostdlib -ffreestanding
ASFLAGS := -DASM -Wall -O2 -ffreestanding -nostdlib

# Configuration -- Uncomment lines to enable option
# Or specify on the commandline

# Enable debugging
PORTURA_DEBUG := y

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

EXE_OBJ := $(objtree)/$(EXE).o

# This is our default target - The default is the first target in the file so
# we need to define this fairly high-up.
all: real-all

PHONY += all install clean dist qemu-test real-all

# This variable defines any extra targets that should be build by 'all'
EXTRA_TARGETS :=

# List of object files to compile into the final .o file (Before boot targets
# are compiled)
OBJS_y :=

# Set configuration options
ifdef V
	Q :=
else
	Q := @
endif

ifdef PROTURA_DEBUG
	CPPFLAGS += -DPROTURA_DEBUG
	CFLAGS += -g
	ASFLAGS += -g
	LDFLAGS += -g
endif

include $(srctree)/config.mk

make_name = $(subst /,_,$(basename $(objtree)/$1))

# Traverse into tree
define subdir_inc
objtree := $$(objtree)/$(1)
srctree := $$(srctree)/$(1)

pfix := $$(subst /,_,$$(objtree))_
DIRINC_y :=

_tmp := $$(shell mkdir -p $$(objtree))
include $$(srctree)/Makefile

$$(foreach subdir,$$(DIRINC_y),$$(eval $$(call subdir_inc,$$(subdir))))

srctree := $$(patsubst %/$(1),%,$$(srctree))
objtree := $$(patsubst %/$(1),%,$$(objtree))
pfix := $$(subst /,_,$$(objtree))_
endef

# Predefine this variable. It contains a list of extra files to clean. Ex.
CLEAN_LIST :=

$(eval $(call subdir_inc,src))
$(eval $(call subdir_inc,arch/$(ARCH)))

# This is a list of targets to create bootable images from
# Ex. For x86, there is a multiboot compliant image, also a full realmode image
# that can boot from a floppy.
BOOT_TARGETS :=

$(eval $(call subdir_inc,arch/$(ARCH)/boot))

_tmp := $(shell mkdir -p $(objtree)/imgs)

define compile_file

_tmp := $$(subst /,_,$$(basename $(1)))_y
ifdef $(_tmp)

CLEAN_LIST += $$($(_tmp))

$(1): $$($(_tmp))
	@echo " LD      $$@"
	$$(Q)$$(LD) -r -o $$@ $$($(_tmp))

endif

endef

$(foreach file,$(OBJS_y),$(eval $(call compile_file,$(file))))


REAL_BOOT_TARGETS :=

define create_boot_target

_expand := $$(objtree)/imgs/$$(EXE)_$$(ARCH)_$(1)
REAL_BOOT_TARGETS += $$(_expand)

CLEAN_LIST += $$(OBJS_$(1))

$$(_expand): $$(EXE_OBJ) $$(OBJS_$(1))
	@echo " CCLD    $$@"
	$$(Q)$$(CC) $$(LDFLAGS) $$(LDFLAGS_$(1)) -o $$@ $$< $$(OBJS_$(1))

endef

$(foreach btarget,$(BOOT_TARGETS),$(eval $(call create_boot_target,$(btarget))))

# Actual entry
real-all: $(REAL_BOOT_TARGETS)

dist: clean
	$(Q)mkdir -p $(EXE)-$(VERSION_N)
	$(Q)cp -R Makefile README.md config.mk LICENSE ./doc ./include ./src ./test $(EXE)-$(VERSION_N)
	$(Q)tar -cf $(EXE)-$(VERSION_N).tar $(EXE)-$(VERSION_N)
	$(Q)gzip $(EXE)-$(VERSION_N).tar
	$(Q)rm -fr $(EXE)-$(VERSION_N)
	@echo " Created $(EXE)-$(VERSION_N).tar.gz"

clean:
	@echo " RM      $(OBJS_y)"
	$(Q)rm -f $(OBJS_y)
	@echo " RM      $(CLEAN_LIST)"
	$(Q)rm -f $(CLEAN_LIST)
	@echo " RM      $(EXE_OBJ)"
	$(Q)rm -f $(EXE_OBJ)

$(EXE_OBJ): $(OBJS_y)
	@echo " LD      $@"
	$(Q)$(LD) -r $(OBJS_y) -o $@

$(objtree)/%.o: $(srctree)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_$(make_name $@)) -c $< -o $@

$(objtree)/%.o: $(srctree)/%.S
	@echo " CCAS    $@"
	$(Q)$(CC) $(CPPFLAGS) $(ASFLAGS) $(ASFLAGS_$(make_name $@)) -o $@ -c $<


.PHONY: $(PHONY)

