/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/scheduler.h>
#include <protura/mm/palloc.h>
#include <protura/mm/vm.h>
#include <arch/idt.h>
#include <protura/drivers/term.h>
#include <arch/task.h>
#include <arch/syscall.h>
#include <arch/cpu.h>
#include <arch/task.h>
#include <arch/drivers/pic8259_timer.h>
#include <protura/fs/sys.h>
#include <protura/net/sys.h>
#include <protura/signal.h>

/* 
 * These simple functions serve as the glue between the underlying
 * functionality used for syscalls, and the actual sys_* functions.
 */

static void sys_handler_putchar(struct irq_frame *frame)
{
    sys_putchar(frame->ebx);
}

static void sys_handler_clock(struct irq_frame *frame)
{
    frame->eax = sys_clock();
}

static void sys_handler_getpid(struct irq_frame *frame)
{
    frame->eax = sys_getpid();
}

static void sys_handler_putint(struct irq_frame *frame)
{
    sys_putint(frame->ebx);
}

static void sys_handler_putstr(struct irq_frame *frame)
{
    sys_putstr((char *)frame->ebx);
}

static void sys_handler_sleep(struct irq_frame *frame)
{
    frame->eax = sys_sleep(frame->ebx);
}

static void sys_handler_fork(struct irq_frame *frame)
{
    frame->eax = sys_fork();
}

static void sys_handler_getppid(struct irq_frame *frame)
{
    frame->eax = sys_getppid();
}

static void sys_handler_open(struct irq_frame *frame)
{
    frame->eax = sys_open((char *)frame->ebx, frame->ecx, frame->edx);
}

static void sys_handler_close(struct irq_frame *frame)
{
    frame->eax = sys_close(frame->ebx);
}

static void sys_handler_read(struct irq_frame *frame)
{
    frame->eax = sys_read(frame->ebx, (char *)frame->ecx, frame->edx);
}

static void sys_handler_write(struct irq_frame *frame)
{
    frame->eax = sys_write(frame->ebx, (char *)frame->ecx, frame->edx);
}

static void sys_handler_lseek(struct irq_frame *frame)
{
    frame->eax = sys_lseek(frame->ebx, frame->ecx, frame->edx);
}

static void sys_handler_execve(struct irq_frame *frame)
{
    sys_execve((char *)frame->ebx, (const char *const *)frame->ecx, (const char *const *)frame->edx, frame);
}

static void sys_handler_yield(struct irq_frame *frame)
{
    scheduler_task_yield();
}

static void sys_handler_exit(struct irq_frame *frame)
{
    sys_exit(frame->ebx);
}

static void sys_handler_wait(struct irq_frame *frame)
{
    frame->eax = sys_wait((int *)frame->ebx);
}

static void sys_handler_dup(struct irq_frame *frame)
{
    frame->eax = sys_dup(frame->ebx);
}

static void sys_handler_dup2(struct irq_frame *frame)
{
    frame->eax = sys_dup2(frame->ebx, frame->ecx);
}

static void sys_handler_brk(struct irq_frame *frame)
{
    sys_brk((va_t)frame->ebx);
}

static void sys_handler_sbrk(struct irq_frame *frame)
{
    frame->eax = (uint32_t)sys_sbrk(frame->ebx);
}

static void sys_handler_read_dent(struct irq_frame *frame)
{
    frame->eax = sys_read_dent(frame->ebx, (struct dent *)frame->ecx, frame->edx);
}

static void sys_handler_chdir(struct irq_frame *frame)
{
    frame->eax = sys_chdir((const char *)frame->ebx);
}

static void sys_handler_truncate(struct irq_frame *frame)
{
    frame->eax = sys_truncate((const char *)frame->ebx, (off_t)frame->ecx);
}

static void sys_handler_ftruncate(struct irq_frame *frame)
{
    frame->eax = sys_ftruncate(frame->ebx, (off_t)frame->ecx);
}

static void sys_handler_link(struct irq_frame *frame)
{
    frame->eax = sys_link((const char *)frame->ebx, (const char *)frame->ecx);
}

static void sys_handler_sync(struct irq_frame *frame)
{
    sys_sync();
}

static void sys_handler_unlink(struct irq_frame *frame)
{
    frame->eax = sys_unlink((const char *)frame->ebx);
}

static void sys_handler_stat(struct irq_frame *frame)
{
    frame->eax = sys_stat((const char *)frame->ebx, (struct stat *)frame->ecx);
}

static void sys_handler_fstat(struct irq_frame *frame)
{
    frame->eax = sys_fstat(frame->ebx, (struct stat *)frame->ecx);
}

static void sys_handler_pipe(struct irq_frame *frame)
{
    frame->eax = sys_pipe((int *)frame->ebx);
}

static void sys_handler_waitpid(struct irq_frame *frame)
{
    frame->eax = sys_waitpid((pid_t)frame->ebx, (int *)frame->ecx, frame->edx);
}

static void sys_handler_sigprocmask(struct irq_frame *frame)
{
    frame->eax = sys_sigprocmask(frame->ebx, (const sigset_t *)frame->ecx, (sigset_t *)frame->edx);
}

static void sys_handler_sigpending(struct irq_frame *frame)
{
    frame->eax = sys_sigpending((sigset_t *)frame->ebx);
}

static void sys_handler_sigaction(struct irq_frame *frame)
{
    frame->eax = sys_sigaction(frame->ebx, (const struct sigaction *)frame->ecx, (struct sigaction *)frame->edx);
}

static void sys_handler_signal(struct irq_frame *frame)
{
    frame->eax = (int)sys_signal(frame->ebx, (sighandler_t)frame->ecx);
}

static void sys_handler_kill(struct irq_frame *frame)
{
    frame->eax = sys_kill((pid_t)frame->ebx, frame->ecx);
}

static void sys_handler_sigwait(struct irq_frame *frame)
{
    frame->eax = sys_sigwait((const sigset_t *)frame->ebx, (int *)frame->ecx);
}

static void sys_handler_sigreturn(struct irq_frame *frame)
{
    sys_sigreturn(frame);
}

static void sys_handler_pause(struct irq_frame *frame)
{
    frame->eax = sys_pause();
}

static void sys_handler_sigsuspend(struct irq_frame *frame)
{
    frame->eax = sys_sigsuspend((const sigset_t *)frame->ebx);
}

static void sys_handler_mkdir(struct irq_frame *frame)
{
    frame->eax = sys_mkdir((const char *)frame->ebx, (mode_t)frame->ecx);
}

static void sys_handler_mknod(struct irq_frame *frame)
{
    frame->eax = sys_mknod((const char *)frame->ebx, (mode_t)frame->ecx, (dev_t)frame->edx);
}

static void sys_handler_rmdir(struct irq_frame *frame)
{
    frame->eax = sys_rmdir((const char *)frame->ebx);
}

static void sys_handler_rename(struct irq_frame *frame)
{
    frame->eax = sys_rename((const char *)frame->ebx, (const char *)frame->ecx);
}

static void sys_handler_readlink(struct irq_frame *frame)
{
    frame->eax = sys_readlink((const char *)frame->ebx, (char *)frame->ecx, (size_t)frame->edx);
}

static void sys_handler_symlink(struct irq_frame *frame)
{
    frame->eax = sys_symlink((const char *)frame->ebx, (const char *)frame->ecx);
}

static void sys_handler_lstat(struct irq_frame *frame)
{
    frame->eax = sys_lstat((const char *)frame->ebx, (struct stat *)frame->ecx);
}

static void sys_handler_mount(struct irq_frame *frame)
{
    frame->eax = sys_mount((const char *)frame->ebx, (const char *)frame->ecx, (const char *)frame->edx, (unsigned long)frame->esi, (const void *)frame->edi);
}

static void sys_handler_umount(struct irq_frame *frame)
{
    frame->eax = sys_umount((const char *)frame->ebx);
}

static void sys_handler_fcntl(struct irq_frame *frame)
{
    frame->eax = sys_fcntl(frame->ebx, frame->ecx, frame->edx);
}

static void sys_handler_time(struct irq_frame *frame)
{
    frame->eax = sys_time((time_t *)frame->ebx);
}

static void sys_handler_setpgid(struct irq_frame *frame)
{
    frame->eax = sys_setpgid(frame->ebx, frame->ecx);
}

static void sys_handler_getpgrp(struct irq_frame *frame)
{
    frame->eax = sys_getpgrp((pid_t *)frame->ebx);
}

static void sys_handler_fork_pgrp(struct irq_frame *frame)
{
    frame->eax = sys_fork_pgrp((pid_t)frame->ebx);
}

static void sys_handler_ioctl(struct irq_frame *frame)
{
    frame->eax = sys_ioctl(frame->ebx, frame->ecx, (uintptr_t)frame->edx);
}

static void sys_handler_poll(struct irq_frame *frame)
{
    frame->eax = sys_poll((struct pollfd *)frame->ebx, (nfds_t)frame->ecx, frame->edx);
}

static void sys_handler_setsid(struct irq_frame *frame)
{
    frame->eax = sys_setsid();
}

static void sys_handler_getsid(struct irq_frame *frame)
{
    frame->eax = sys_getsid(frame->ebx);
}

static void sys_handler_socket(struct irq_frame *frame)
{
    frame->eax = sys_socket(frame->ebx, frame->ecx, frame->edx);
}

static void sys_handler_sendto(struct irq_frame *frame)
{
    frame->eax = sys_sendto(frame->ebx, (const void *)frame->ecx, (size_t)frame->edx, frame->esi, (const struct sockaddr *)frame->edi, (socklen_t)frame->ebp);
}

static void sys_handler_recvfrom(struct irq_frame *frame)
{
    frame->eax = sys_recvfrom(frame->ebx, (void *)frame->ecx, (size_t)frame->edx, frame->esi, (struct sockaddr *)frame->edi, (socklen_t *)frame->ebp);
}

static void sys_handler_bind(struct irq_frame *frame)
{
    frame->eax = sys_bind(frame->ebx, (const struct sockaddr *)frame->ecx, (socklen_t)frame->edx);
}

static void sys_handler_setsockopt(struct irq_frame *frame)
{
    frame->eax = sys_setsockopt(frame->ebx, frame->ecx, frame->edx, (const void *)frame->esi, (socklen_t)frame->edi);
}

static void sys_handler_getsockopt(struct irq_frame *frame)
{
    frame->eax = sys_getsockopt(frame->ebx, frame->ecx, frame->edx, (void *)frame->esi, (socklen_t *)frame->edi);
}

static void sys_handler_shutdown(struct irq_frame *frame)
{
    frame->eax = sys_shutdown(frame->ebx, frame->ecx);
}

static void sys_handler_getsockname(struct irq_frame *frame)
{
    frame->eax = sys_getsockname(frame->ebx, (struct sockaddr *)frame->ecx, (socklen_t *)frame->edx);
}

static void sys_handler_send(struct irq_frame *frame)
{
    frame->eax = sys_send(frame->ebx, (const void *)frame->ecx, (size_t)frame->edx, frame->esi);
}

static void sys_handler_recv(struct irq_frame *frame)
{
    frame->eax = sys_recv(frame->ebx, (void *)frame->ecx, (size_t)frame->edx, frame->esi);
}

static void sys_handler_gettimeofday(struct irq_frame *frame)
{
    frame->eax = sys_gettimeofday((struct timeval *)frame->ebx, (struct timezone *)frame->ecx);
}

static void sys_handler_setuid(struct irq_frame *frame)
{
    frame->eax = sys_setuid((uid_t)frame->ebx);
}

#define SYSCALL(call, handler) \
    [SYSCALL_##call] = { SYSCALL_##call, handler }

static struct syscall_handler {
    int num;
    void (*handler) (struct irq_frame *);
} syscall_handlers[] = {
    SYSCALL(PUTCHAR, sys_handler_putchar),
    SYSCALL(CLOCK, sys_handler_clock),
    SYSCALL(GETPID, sys_handler_getpid),
    SYSCALL(PUTINT, sys_handler_putint),
    SYSCALL(PUTSTR, sys_handler_putstr),
    SYSCALL(SLEEP, sys_handler_sleep),
    SYSCALL(FORK, sys_handler_fork),
    SYSCALL(GETPPID, sys_handler_getppid),
    SYSCALL(OPEN, sys_handler_open),
    SYSCALL(CLOSE, sys_handler_close),
    SYSCALL(READ, sys_handler_read),
    SYSCALL(WRITE, sys_handler_write),
    SYSCALL(LSEEK, sys_handler_lseek),
    SYSCALL(EXECVE, sys_handler_execve),
    SYSCALL(YIELD, sys_handler_yield),
    SYSCALL(EXIT, sys_handler_exit),
    SYSCALL(WAIT, sys_handler_wait),
    SYSCALL(DUP, sys_handler_dup),
    SYSCALL(DUP2, sys_handler_dup2),
    SYSCALL(BRK, sys_handler_brk),
    SYSCALL(SBRK, sys_handler_sbrk),
    SYSCALL(READ_DENT, sys_handler_read_dent),
    SYSCALL(CHDIR, sys_handler_chdir),
    SYSCALL(TRUNCATE, sys_handler_truncate),
    SYSCALL(FTRUNCATE, sys_handler_ftruncate),
    SYSCALL(LINK, sys_handler_link),
    SYSCALL(SYNC, sys_handler_sync),
    SYSCALL(UNLINK, sys_handler_unlink),
    SYSCALL(STAT, sys_handler_stat),
    SYSCALL(FSTAT, sys_handler_fstat),
    SYSCALL(PIPE, sys_handler_pipe),
    SYSCALL(WAITPID, sys_handler_waitpid),
    SYSCALL(SIGPROCMASK, sys_handler_sigprocmask),
    SYSCALL(SIGPENDING, sys_handler_sigpending),
    SYSCALL(SIGACTION, sys_handler_sigaction),
    SYSCALL(SIGNAL, sys_handler_signal),
    SYSCALL(KILL, sys_handler_kill),
    SYSCALL(SIGWAIT, sys_handler_sigwait),
    SYSCALL(SIGRETURN, sys_handler_sigreturn),
    SYSCALL(PAUSE, sys_handler_pause),
    SYSCALL(SIGSUSPEND, sys_handler_sigsuspend),
    SYSCALL(MKDIR, sys_handler_mkdir),
    SYSCALL(MKNOD, sys_handler_mknod),
    SYSCALL(RMDIR, sys_handler_rmdir),
    SYSCALL(RENAME, sys_handler_rename),
    SYSCALL(READLINK, sys_handler_readlink),
    SYSCALL(SYMLINK, sys_handler_symlink),
    SYSCALL(LSTAT, sys_handler_lstat),
    SYSCALL(MOUNT, sys_handler_mount),
    SYSCALL(UMOUNT, sys_handler_umount),
    SYSCALL(FCNTL, sys_handler_fcntl),
    SYSCALL(TIME, sys_handler_time),
    SYSCALL(SETPGID, sys_handler_setpgid),
    SYSCALL(GETPGRP, sys_handler_getpgrp),
    SYSCALL(FORK_PGRP, sys_handler_fork_pgrp),
    SYSCALL(IOCTL, sys_handler_ioctl),
    SYSCALL(POLL, sys_handler_poll),
    SYSCALL(SETSID, sys_handler_setsid),
    SYSCALL(GETSID, sys_handler_getsid),
    SYSCALL(SOCKET, sys_handler_socket),
    SYSCALL(SENDTO, sys_handler_sendto),
    SYSCALL(RECVFROM, sys_handler_recvfrom),
    SYSCALL(BIND, sys_handler_bind),
    SYSCALL(SETSOCKOPT, sys_handler_setsockopt),
    SYSCALL(GETSOCKOPT, sys_handler_getsockopt),
    SYSCALL(GETSOCKNAME, sys_handler_getsockname),
    SYSCALL(SHUTDOWN, sys_handler_shutdown),
    SYSCALL(SEND, sys_handler_send),
    SYSCALL(RECV, sys_handler_recv),
    SYSCALL(GETTIMEOFDAY, sys_handler_gettimeofday),
    SYSCALL(SETUID, sys_handler_setuid),
};

static void syscall_handler(struct irq_frame *frame, void *param)
{
    if (frame->eax < ARRAY_SIZE(syscall_handlers))
        (syscall_handlers[frame->eax].handler) (frame);
}

void syscall_init(void)
{
    irq_register_callback(INT_SYSCALL, syscall_handler, "syscall", IRQ_SYSCALL, NULL);
}

