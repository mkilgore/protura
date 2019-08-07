
.PHONY: extra-ncurses
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

DISK_CLEAN_EXTRA += ./disk/extras/ncurses.built

.PHONY: extra-vim
extra-vim: extra-ncurses disk/extras/vim.built
disk/extras/vim.built:
	@echo " BUILD   vim"
	$(Q)cd disk/extras/; ./build_vim.sh $(TARGET) "$(DISK_ROOT)" "/usr"
	@echo " TOUCH   $@"
	$(Q)touch ./disk/extras/vim.built

DISK_CLEAN_EXTRA += ./disk/extras/vim.built

.PHONY: extra-ed
extra-ed: disk/extras/ed.built
disk/extras/ed.built:
	@echo " BUILD   ed"
	$(Q)cd ./disk/extras/ed; ./configure \
		"CC-$(CC)" \
		--prefix=/usr
	$(Q)cd ./disk/extras/ed; make
	$(Q)cd ./disk/extras/ed; make install "DESTDIR=$(DISK_ROOT)"
	@echo " TOUCH   $@"
	$(Q)touch ./disk/extras/ed.built
