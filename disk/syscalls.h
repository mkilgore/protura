#ifndef DISK_SYSCALLS_H
#define DISK_SYSCALLS_H

#include <protura/types.h>
#include <protura/syscall.h>
#include <fs/stat.h>
#include <fs/fcntl.h>

static inline int syscall3(int sys, int arg1, int arg2, int arg3)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2), "d" (arg3)
                 : "memory");

    return out;
}

static inline int syscall2(int sys, int arg1, int arg2)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1), "c" (arg2)
                 : "memory");

    return out;
}

static inline int syscall1(int sys, int arg1)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys), "b" (arg1)
                 : "memory");

    return out;
}

static inline int syscall0(int sys)
{
    int out;
    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (out)
                 : "0" (sys)
                 : "memory");

    return out;
}

static inline int putchar(int ch)
{
    return syscall1(SYSCALL_PUTCHAR, ch);
}

static inline int putstr(const char *str)
{
    return syscall1(SYSCALL_PUTSTR, (int)str);
}

static inline int putint(int val)
{
    return syscall1(SYSCALL_PUTINT, val);
}

static inline int getpid(void)
{
    return syscall0(SYSCALL_GETPID);
}

static inline int open(const char *prog, int flags, int mode)
{
    return syscall3(SYSCALL_OPEN, (int)prog, flags, mode);
}

static inline int close(int fd)
{
    return syscall1(SYSCALL_CLOSE, fd);
}

static inline int read(int fd, char *buf, int len)
{
    return syscall3(SYSCALL_READ, fd, (int)buf, len);
}

static inline int write(int fd, char *buf, int len)
{
    return syscall3(SYSCALL_WRITE, fd, (int)buf, len);
}

static inline int msleep(int mseconds)
{
    return syscall1(SYSCALL_SLEEP, mseconds);
}

static inline int fork(void)
{
    return syscall0(SYSCALL_FORK);
}

static inline int exec(const char *prog)
{
    return syscall1(SYSCALL_EXEC, (int)prog);
}

static inline void yield(void)
{
    syscall0(SYSCALL_YIELD);
}

static inline void exit(int code)
{
    syscall1(SYSCALL_EXIT, code);
}

static inline pid_t wait(int *code)
{
    return syscall1(SYSCALL_WAIT, (int)code);
}

static inline int dup(int oldfd)
{
    return syscall1(SYSCALL_DUP, oldfd);
}

static inline int dup2(int oldfd, int newfd)
{
    return syscall2(SYSCALL_DUP2, oldfd, newfd);
}

static inline void brk(void *new_brk)
{
    syscall1(SYSCALL_BRK, (int)new_brk);
}

static inline void *sbrk(intptr_t increment)
{
    return (void *)syscall1(SYSCALL_SBRK, increment);
}

#endif
