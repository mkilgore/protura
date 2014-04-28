
OBJS_y += gdt.o
OBJS_y += gdt_flush.o
OBJS_y += irq_handler.o
OBJS_y += idt.o
OBJS_y += irq_array.o

$(objtree)/irq_array.S: $(srctree)/irq_array_gen.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

CLEAN_LIST_y += irq_array.S

