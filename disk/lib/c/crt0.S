
#include <protura/syscall.h>

.extern main

/* Entry point for a C program. Note that argc and argv are already on the
 * stack from the kernel. */
.globl _start
_start:
    call main
    mov %eax, %ebx
    mov $SYSCALL_EXIT, %eax
    int $INT_SYSCALL
1:
    jmp 1b

