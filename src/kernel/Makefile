
objs-y += kmain.o
objs-y += string.o
objs-y += debug.o
objs-y += basic_printf.o
objs-y += sprintf.o
objs-y += char_buf.o
objs-y += dump_mem.o
objs-y += strtol.o
objs-y += scheduler.o
objs-y += strerror.o

$(eval $(call add_dep,$(objtree)/strerror.c,$(objtree)/errors.x))
$(eval $(call create_rule,$(objtree)/errors.x,./include/protura/errors.h,PERL    ,$$(Q)$$(PERL) $(objtree)/strerror.pl < $$< > $$@))

clean-list-y += errors.x

