
SH_SRCS := \
	./disk/utils/sh/sh.c \
	./disk/utils/sh/prog.c \
	./disk/utils/sh/shell.c \
	./disk/utils/sh/input.c \
	./disk/utils/sh/input_lexer.c \
	./disk/utils/sh/arg_parser.c

SH_OBJS := $(SH_SRCS:.c=.o)

./disk/utils/sh.o: $(SH_OBJS)
	@echo " LD      $@"
	$(Q)$(LD) -r -o $@ $^

DISK_UTIL_OBJS += ./disk/utils/sh.o
DISK_UTIL_EXTRA_OBJS := $(SH_OBJS)

