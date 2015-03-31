
# objs-y += gdt.o
objs-y += cpu.o
# objs-y += gdt_flush.o
objs-y += task.o
objs-y += fake_task.o
objs-y += idle_task.o
objs-y += irq_handler.o
objs-y += idt.o
objs-y += irq_array.o

$(objtree)/irq_array.S: $(srctree)/irq_array_gen.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

clean-list-y += irq_array.S

