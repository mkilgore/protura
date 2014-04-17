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

CPPFLAGS := -DPROTURA_VERSION=$(VERSION)              \
            -DPROTURA_SUBLEVEL=$(SUBLEVEL)            \
            -DPROTURA_PATCH=$(PATCH)                  \
            -DPROTURA_VERSION_N="$(VERSION_N)"        \
            -DPROTURA_ARCH="$(ARCH)"                  \
			-DBITS=$(BITS)                            \
            -I'./include' -I'./arch/$(ARCH)/include'

CFLAGS  := -Wall -O2 -std=gnu99 -ffreestanding        \
           -fno-strict-aliasing -nostdlib

LDFLAGS := -ffreestanding -nostdlib -T ./arch/$(ARCH)/linker.ld
ASFLAGS := -DASM -Wall -O2 -ffreestanding -nostdlib

# Configuration -- Uncomment lines to enable option
# Or specify on the commandline

# Enable debugging
# PORTURA_DEBUG := y

# Show all commands executed by the Makefile
# V := y

srctree := .

# Define 'output_dir' if you want everything to be built in an outside
# directory
ifdef output_dir
	objtree := $(output_dir)
else
	objtree := .
endif

PHONY += all install clean dist qemu-test

MAKEFLAGS += -rR --no-print-directory

OBJS_y :=

# Set configuration options
ifdef V
	Q :=
else
	Q := @
endif

ifdef PROTURA_DEBUG
	CFLAGS += -DPROTURA_DEBUG -g
endif

include $(srctree)/config.mk

make_name = $(subst /,_,$(basename $(objtree)/$1))

# Traverse into tree
define subdir_inc
objtree := $$(objtree)/$(1)
srctree := $$(srctree)/$(1)

pfix := $$(subst /,_,$$(objtree))_

_tmp := $$(shell mkdir -p $$(objtree))
include $$(srctree)/Makefile

srctree := $$(patsubst %/$(1),%,$$(srctree))
objtree := $$(patsubst %/$(1),%,$$(objtree))
pfix := $$(subst /,_,$$(objtree))_
endef

$(eval $(call subdir_inc,src))
$(eval $(call subdir_inc,arch/$(ARCH)))

all: $(objtree)/$(EXE)

# Predefine this variable. It contains a list of extra files to clean. Ex.
CLEAN_LIST :=

define compile_file

_tmp := $$(subst /,_,$$(basename $(1)))_y
ifdef $$(_tmp)

	CLEAN_LIST += $$($$(_tmp))

$(1): $$($$(subst /,_,$$(basename $(1)))_y)
	@echo " LD      $$@"
	$$(Q)$$(LD) -r -o $$@ $$($$(subst /,_,$$(basename $(1)))_y)

endif

endef

$(foreach file,$(OBJS_y),$(eval $(call compile_file,$(file))))

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
	@echo " RM      $(EXE)"
	$(Q)rm -f $(EXE)

$(objtree)/$(EXE): $(OBJS_y)
	@echo " CCLD    $@"
	$(Q)$(CC) $(LDFLAGS) $(OBJS_y) -o $@

$(objtree)/%.o: $(srctree)/%.c
	@echo " CC      $@"
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) $(CFLAGS_$(make_name $@)) -c $< -o $@

$(objtree)/%.o: $(objtree)/%.S
	@echo " CC      $@"
	$(Q)$(CC) $(CPPFLAGS) $(ASFLAGS) $(ASFLAGS_$(make_name $@)) -o $@ -c $<

qemu-test: $(objtree)/$(EXE)
	@echo " Testing with QEMU."
	$(Q)qemu-system-i386 -monitor stdio -kernel $(objtree)/$(EXE)

.PHONY: $(PHONY)

