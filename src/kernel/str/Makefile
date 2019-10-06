
objs-y += string.o
objs-y += basic_printf.o
objs-y += sprintf.o
objs-y += strtol.o
objs-y += strerror.o

$(eval $(call add_dep,$(tree)/strerror.c,$(tree)/errors.x))
$(eval $(call create_rule,$(tree)/errors.x,./include/protura/errors.h,PERL    ,$$(Q)$$(PERL) $(tree)/strerror.pl < $$< > $$@))

clean-list-y += errors.x


