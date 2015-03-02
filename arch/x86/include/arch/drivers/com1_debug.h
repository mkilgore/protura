#ifndef INCLUDE_ARCH_DRIVERS_COM1_DEBUG_H
#define INCLUDE_ARCH_DRIVERS_COM1_DEBUG_H

#include <protura/compiler.h>
#include <protura/stdarg.h>

void com1_init(void);
void com1_putchar(char c);
void com1_putstr(const char *s);

void com1_printfv(const char *s, va_list lst);
void com1_printf(const char *s, ...) __printf(1, 2);

#endif