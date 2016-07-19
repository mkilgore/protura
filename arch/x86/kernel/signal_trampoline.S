
#include <protura/syscall.h>

.section .data

trampoline_start:
    movl $SYSCALL_SIGRETURN, %eax
    int $INT_SYSCALL
trampoline_end:

.globl x86_trampoline_code
x86_trampoline_code:
    .long trampoline_start

.globl x86_trampoline_len
x86_trampoline_len:
    .long trampoline_end - trampoline_start


