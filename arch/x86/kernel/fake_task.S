
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

    popl %ebx

1:  movl $SYSCALL_PUTCHAR, %eax
    int $INT_SYSCALL

    movl $SYSCALL_CLOCK, %eax
    int $INT_SYSCALL

.globl right_before_set
right_before_set:
    shll $16, %eax
    andl $0x003FFFFF, %eax
    orl $0x00100000, %eax

    movl %eax, %ecx
2:  loop 2b

    jmp 1b

fake_task_end:

.globl fake_task_size
fake_task_size:
    .long fake_task_end - fake_task_entry

