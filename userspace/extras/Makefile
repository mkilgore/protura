
EXTRA_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
EXTRA_DIR := $(EXTRA_DIR:/=)

EXTRAS_PREFIX := /usr

.PHONY: extra-ncurses
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/ncurses.built
extra-ncurses: $(EXTRA_DIR)/ncurses.built
$(EXTRA_DIR)/ncurses.built:
	@echo " BUILD   ncurses"
	$(Q)cd $(EXTRA_DIR); ./build_ncurses.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $(EXTRA_DIR)/ncurses.built

.PHONY: extra-vim
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/vim.built
extra-vim: $(EXTRA_DIR)/vim.built
$(EXTRA_DIR)/vim.built: $(EXTRA_DIR)/ncurses.built
	@echo " BUILD   vim"
	$(Q)cd $(EXTRA_DIR); ./build_vim.sh $(TARGET) "$(DISK_ROOT)" "/usr"
	@echo " TOUCH   $@"
	$(Q)touch $(EXTRA_DIR)/vim.built

.PHONY: extra-less
DISK_CLEAN_EXTRA += ./disk/extra/less.built
extra-less: $(EXTRA_DIR)/less.built
$(EXTRA_DIR)/less.built: $(EXTRA_DIR)/ncurses.built
	@echo " BUILD   less"
	$(Q)cd $(EXTRA_DIR); ./build_less.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-ed
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/ed.built
extra-ed: $(EXTRA_DIR)/ed.built
$(EXTRA_DIR)/ed.built:
	@echo " BUILD   ed"
	$(Q)cd $(EXTRA_DIR); ./build_ed.sh $(TARGET) "$(DISK_ROOT)" "/usr"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-gmp
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/gmp.built
extra-gmp: $(EXTRA_DIR)/gmp.built
$(EXTRA_DIR)/gmp.built:
	@echo " BUILD   gmp"
	$(Q)cd $(EXTRA_DIR); ./build_gmp.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-mpfr
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/mpfr.built
extra-mpfr: $(EXTRA_DIR)/mpfr.built
$(EXTRA_DIR)/mpfr.built:
	@echo " BUILD   mpfr"
	$(Q)cd $(EXTRA_DIR); ./build_mpfr.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-mpc
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/mpc.built
extra-mpc: $(EXTRA_DIR)/mpc.built
$(EXTRA_DIR)/mpc.built: $(EXTRA_DIR)/gmp.built $(EXTRA_DIR)/mpfr.built
	@echo " BUILD   mpc"
	$(Q)cd $(EXTRA_DIR); ./build_mpc.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-binutils
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/binutils.built
extra-binutils: $(EXTRA_DIR)/binutils.built
$(EXTRA_DIR)/binutils.built:
	@echo " BUILD   binutils"
	$(Q)cd $(EXTRA_DIR); ./build_binutils.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-gcc
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/gcc.built
extra-gcc: $(EXTRA_DIR)/gcc.built
$(EXTRA_DIR)/gcc.built: $(EXTRA_DIR)/gmp.built $(EXTRA_DIR)/mpfr.built $(EXTRA_DIR)/mpc.built
	@echo " BUILD   gcc"
	$(Q)cd $(EXTRA_DIR); ./build_gcc.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-gbemuc
DISK_CLEAN_EXTRA += $(EXTRA_DIR)/gbemuc.built
extra-gbemuc: $(EXTRA_DIR)/gbemuc.built
$(EXTRA_DIR)/gbemuc.built:
	@echo " BUILD   gbemuc"
	$(Q)cd $(EXTRA_DIR); ./build_gbemuc.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

DISK_EXTRA_PROGS := $(wildcard $(EXTRA_DIR)/*.built)
DISK_CLEAN += $(DISK_CLEAN_EXTRA)
