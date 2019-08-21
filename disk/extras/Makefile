
EXTRAS_PREFIX := /usr

.PHONY: extra-ncurses
DISK_CLEAN_EXTRA += ./disk/extras/ncurses.built
extra-ncurses: disk/extras/ncurses.built
disk/extras/ncurses.built:
	@echo " BUILD   ncurses"
	$(Q)cd disk/extras/ncurses; ./configure \
			--host=$(TARGET) \
			--without-tests \
			--with-normal \
			--with-install-prefix=$(DISK_ROOT) \
			--prefix=/usr;
	$(Q)cd disk/extras/ncurses; $(MAKE)
	$(Q)cd disk/extras/ncurses; $(MAKE) install
	@echo " TOUCH   $@"
	$(Q)touch ./disk/extras/ncurses.built

.PHONY: extra-vim
DISK_CLEAN_EXTRA += ./disk/extras/vim.built
extra-vim: extra-ncurses disk/extras/vim.built
disk/extras/vim.built:
	@echo " BUILD   vim"
	$(Q)cd disk/extras/; ./build_vim.sh $(TARGET) "$(DISK_ROOT)" "/usr"
	@echo " TOUCH   $@"
	$(Q)touch ./disk/extras/vim.built

.PHONY: extra-ed
DISK_CLEAN_EXTRA += ./disk/extras/ed.built
extra-ed: disk/extras/ed.built
disk/extras/ed.built:
	@echo " BUILD   ed"
	$(Q)cd ./disk/extras/ed; ./configure \
		"CC-$(CC)" \
		--prefix=$(EXTRAS_PREFIX)
	$(Q)cd ./disk/extras/ed; make
	$(Q)cd ./disk/extras/ed; make install "DESTDIR=$(DISK_ROOT)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-gmp
DISK_CLEAN_EXTRA += ./disk/extras/gmp.built
extra-gmp: disk/extras/gmp.built
disk/extras/gmp.built:
	@echo " BUILD   gmp"
	$(Q)cd disk/extras/; ./build_gmp.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-mpfr
DISK_CLEAN_EXTRA += ./disk/extras/mpfr.built
extra-mpfr: disk/extras/mpfr.built
disk/extras/mpfr.built:
	@echo " BUILD   mpfr"
	$(Q)cd disk/extras/; ./build_mpfr.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-mpc
DISK_CLEAN_EXTRA += ./disk/extras/mpc.built
extra-mpc: extra-gmp extra-mpfr disk/extras/mpc.built
disk/extras/mpc.built:
	@echo " BUILD   mpc"
	$(Q)cd disk/extras/; ./build_mpc.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-binutils
DISK_CLEAN_EXTRA += ./disk/extras/binutils.built
extra-binutils: disk/extras/binutils.built
disk/extras/binutils.built:
	@echo " BUILD   binutils"
	$(Q)cd disk/extras/; ./build_binutils.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

.PHONY: extra-gcc
DISK_CLEAN_EXTRA += ./disk/extras/gcc.built
extra-gcc: extra-gmp extra-mpc extra-mpfr disk/extras/gcc.built
disk/extras/gcc.built:
	@echo " BUILD   gcc"
	$(Q)cd disk/extras/; ./build_gcc.sh "$(TARGET)" "$(DISK_ROOT)" "$(EXTRAS_PREFIX)" "$(MAKEFLAGS)"
	@echo " TOUCH   $@"
	$(Q)touch $@

