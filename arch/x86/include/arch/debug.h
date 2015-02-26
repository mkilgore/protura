#ifndef INCLUDE_ARCH_DEBUG_H
#define INCLUDE_ARCH_DEBUG_H

#include <arch/drivers/com1_debug.h>

#define arch_printfv(...) com1_printfv(__VA_ARGS__)
#define arch_printf(...)  com1_printf(__VA_ARGS__)

#endif
