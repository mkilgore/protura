
OBJS_y += $(objtree)/gdt.o
OBJS_y += $(objtree)/gdt_flush.o
OBJS_y += $(objtree)/irq_handler.o
OBJS_y += $(objtree)/idt.o
OBJS_y += $(objtree)/irq_array.o

$(objtree)/irq_array.S: $(objtree)/irq_array_gen.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

CLEAN_LIST += $(objtree)/irq_array.S

