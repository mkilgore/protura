
objs-y += cpu.o
objs-y += cpuid.o
objs-y += task.o
objs-y += kernel_task.o
objs-y += task_user_entry.o
objs-y += context.o
objs-y += backtrace.o
objs-y += irq_handler.o
objs-y += idt.o
objs-y += irq_array.o
objs-y += string.o
objs-y += signal.o
objs-y += signal_trampoline.o

$(objtree)/irq_array.S: $(srctree)/irq_array_gen.pl
	@echo " PERL    $@"
	$(Q)$(PERL) $< > $@

clean-list-y += irq_array.S

