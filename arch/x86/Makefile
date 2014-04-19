
OBJS_y += $(objtree)/gdt.o $(objtree)/gdt_flush.o $(objtree)/init.o $(objtree)/irq_handler.o $(objtree)/idt.o $(objtree)/irq_array.o

$(objtree)/irq_array.S: $(objtree)/irq_array_gen.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

CLEAN_LIST += $(objtree)/irq_array.S

DIRINC_y += drivers

