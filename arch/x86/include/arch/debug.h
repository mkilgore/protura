#ifndef INCLUDE_ARCH_DEBUG_H
#define INCLUDE_ARCH_DEBUG_H

#include <arch/drivers/kprintf.h>

#define arch_printfv(...) arch_kprintfv(__VA_ARGS__)
#define arch_printf(...)  arch_kprintf(__VA_ARGS__)

#endif
