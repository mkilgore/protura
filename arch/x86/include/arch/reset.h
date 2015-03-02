#ifndef INCLUDE_ARCH_RESET_H
#define INCLUDE_ARCH_RESET_H

#include <protura/compiler.h>

void system_reboot(void) __noreturn;
void system_shutdown(void) __noreturn;

#endif
