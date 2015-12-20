#ifndef DISK_SYSCALLS_H
#define DISK_SYSCALLS_H

#include <protura/types.h>
#include <protura/syscall.h>
#include <protura/fs/stat.h>
#include <protura/fs/fcntl.h>

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

static inline int write(int fd, const char *buf, int len)
{
    return syscall3(SYSCALL_WRITE, fd, (int)buf, len);
}

static inline off_t lseek(int fd, off_t offset, int whence)
{
    return syscall3(SYSCALL_LSEEK, fd, (int)offset, whence);
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
    return syscall3(SYSCALL_EXECVE, (int)prog, 0, 0);
}

static inline int execv(const char *prog, const char *const *argv)
{
    return syscall3(SYSCALL_EXECVE, (int)prog, (int)argv, 0);
}

static inline int execve(const char *prog, const char *const *argv, const char *const *envp)
{
    return syscall3(SYSCALL_EXECVE, (int)prog, (int)argv, (int)envp);
}

static inline void yield(void)
{
    syscall0(SYSCALL_YIELD);
}

static inline void _exit(int code)
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

static inline int read_dent(int fd, void *ptr, size_t size)
{
    return syscall3(SYSCALL_READ_DENT, fd, (int)ptr, (int)size);
}

static inline int chdir(const char *path)
{
    return syscall1(SYSCALL_CHDIR, (int)path);
}

static inline int truncate(const char *path, off_t length)
{
    return syscall2(SYSCALL_TRUNCATE, (int)path, (int)length);
}

static inline int ftruncate(int fd, off_t length)
{
    return syscall2(SYSCALL_FTRUNCATE, fd, (int)length);
}

static inline int link(const char *old, const char *new)
{
    return syscall2(SYSCALL_LINK, (int)old, (int)new);
}

static inline void sync(void)
{
    syscall0(SYSCALL_SYNC);
}

#endif
