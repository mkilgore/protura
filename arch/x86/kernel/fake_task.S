
#include <arch/syscall.h>

str1:
    .string "My pid: \0"
str2:
    .string "My DPL: \0"

newl:
    .string "\n\0"


.extern term_putchar
.globl fake_task_entry
fake_task_entry:
    pushl %eax /* Push the char to print, we'll pop it into %ebx later */

    movl $str1, %ebx
    movl $SYSCALL_PUTSTR, %eax
    int $INT_SYSCALL

    movl $SYSCALL_GETPID, %eax
    int $INT_SYSCALL

    movl %eax, %ebx
    movl $SYSCALL_PUTINT, %eax
    int $INT_SYSCALL

    movl $newl, %ebx
    movl $SYSCALL_PUTSTR, %eax
    int $INT_SYSCALL

    movl $str2, %ebx
    movl $SYSCALL_PUTSTR, %eax
    int $INT_SYSCALL

    xorl %ebx, %ebx
    movw %cs, %bx
    andl $0x03, %ebx
    movl $SYSCALL_PUTINT, %eax
    int $INT_SYSCALL

    movl $newl, %ebx
    movl $SYSCALL_PUTSTR, %eax
    int $INT_SYSCALL

    popl %ecx

1:  movl %ecx, %ebx
    movl $SYSCALL_PUTCHAR, %eax
    int $INT_SYSCALL

    movl $2000, %ebx
    movl $SYSCALL_SLEEP, %eax
    int $INT_SYSCALL

    jmp 1b

fake_task_end:

.globl fake_task_size
fake_task_size:
    .long fake_task_end - fake_task_entry

