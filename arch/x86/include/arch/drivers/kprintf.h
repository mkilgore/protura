#ifndef INCLUDE_ARCH_DRIVERS_KPRINTF_H
#define INCLUDE_ARCH_DRIVERS_KPRINTF_H

#include <protura/stdarg.h>

void arch_kprintfv(const char *fmt, va_list lst);
void arch_kprintf(const char *fmt, ...);

#endif
