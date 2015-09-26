
#include <arch/syscall.h>

.text
str1:
    .string "My pid: \0"
str2:
    .string "My DPL: \0"
str3:
    .string "My Parent: \0"

newl:
    .string "\n\0"

forkstr:
    .string "Forking...\n\0"
childpid:
    .string "Child pid: \0"
childstr:
    .string "In child! Pid: \0"
pid_prefix:
    .string ": \0"

.macro SYSCALL sys
    movl $\sys, %eax
    int $INT_SYSCALL
.endm

.macro WRITE_STR str
    movl $\str, %ebx
    SYSCALL SYSCALL_PUTSTR
.endm

.macro PID_PREFIX pid
    movl \pid, %ebx
    SYSCALL SYSCALL_PUTINT
    WRITE_STR pid_prefix
.endm


.extern term_putchar
.globl fake_task_entry
fake_task_entry:

    pushl %eax /* Push the char to print, we'll pop it into %ebx later */

    /* Display 'My Pid: pid' */
    SYSCALL SYSCALL_GETPID
    movl %eax, %edi /* Store our pid in %edi for future use */

    PID_PREFIX %edi
    WRITE_STR str1
    movl %edi, %ebx
    SYSCALL SYSCALL_PUTINT
    WRITE_STR newl

    SYSCALL SYSCALL_FORK

    popl %ecx

    /* Conditionally jump to 'child' or 'parent' code, depending on if we're
     * currently the child or parent from 'fork()'.
     *
     * If we're the child, then fork returns zero, so we can just cmp %eax to
     * check */
    cmp $0, %eax
    je child

parent:
    /* Save child pid */
    movl %eax, %edx

    PID_PREFIX %edi
    WRITE_STR childpid
    movl %edx, %ebx
    SYSCALL SYSCALL_PUTINT
    WRITE_STR newl

    jmp charloop

child:
    /* Get our new pid */
    SYSCALL SYSCALL_GETPID
    movl %eax, %edi /* Store our pid in %edi for future use */

    incl %ecx

    PID_PREFIX %edi
    WRITE_STR childstr
    movl %edi, %ebx
    SYSCALL SYSCALL_PUTINT
    movl %eax, %ebx
    SYSCALL SYSCALL_PUTINT
    WRITE_STR newl

    /* Display 'My Parent: pid' */
    PID_PREFIX %edi
    WRITE_STR str3
    SYSCALL SYSCALL_GETPPID
    movl %eax, %ebx
    SYSCALL SYSCALL_PUTINT
    WRITE_STR newl

    /* Fall through to charloop */

charloop:
1:  movl %ecx, %ebx
    SYSCALL SYSCALL_PUTCHAR

    movl $1000, %ebx
    SYSCALL SYSCALL_SLEEP

    jmp 1b

fake_task_end:

.globl fake_task_size
fake_task_size:
    .long fake_task_end - fake_task_entry

