/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_ARCH_SYSCALL_H
#define INCLUDE_ARCH_SYSCALL_H

#define INT_SYSCALL 0x81

#define SYSCALL_PUTCHAR      0x01
#define SYSCALL_CLOCK        0x02
#define SYSCALL_GETPID       0x03
#define SYSCALL_PUTINT       0x04
#define SYSCALL_PUTSTR       0x05
#define SYSCALL_SLEEP        0x06
#define SYSCALL_FORK         0x07
#define SYSCALL_GETPPID      0x08
#define SYSCALL_OPEN         0x09
#define SYSCALL_CLOSE        0x0A
#define SYSCALL_READ         0x0B
#define SYSCALL_WRITE        0x0C
#define SYSCALL_LSEEK        0x0D
#define SYSCALL_EXECVE       0x0E
#define SYSCALL_YIELD        0x0F
#define SYSCALL_EXIT         0x10
#define SYSCALL_WAIT         0x11
#define SYSCALL_DUP          0x12
#define SYSCALL_DUP2         0x13
#define SYSCALL_BRK          0x14
#define SYSCALL_SBRK         0x15
#define SYSCALL_READ_DENT    0x16
#define SYSCALL_CHDIR        0x17
#define SYSCALL_TRUNCATE     0x18
#define SYSCALL_FTRUNCATE    0x19
#define SYSCALL_LINK         0x1A
#define SYSCALL_SYNC         0x1B
#define SYSCALL_UNLINK       0x1C
#define SYSCALL_STAT         0x1D
#define SYSCALL_FSTAT        0x1E
#define SYSCALL_PIPE         0x1F
#define SYSCALL_WAITPID      0x20
#define SYSCALL_SIGPROCMASK  0x21
#define SYSCALL_SIGPENDING   0x22
#define SYSCALL_SIGACTION    0x23
#define SYSCALL_SIGNAL       0x24
#define SYSCALL_KILL         0x25
#define SYSCALL_SIGWAIT      0x26
#define SYSCALL_SIGRETURN    0x27
#define SYSCALL_PAUSE        0x28
#define SYSCALL_SIGSUSPEND   0x29
#define SYSCALL_MKDIR        0x2A
#define SYSCALL_MKNOD        0x2B
#define SYSCALL_RMDIR        0x2C
#define SYSCALL_RENAME       0x2D
#define SYSCALL_READLINK     0x2E
#define SYSCALL_SYMLINK      0x2F
#define SYSCALL_LSTAT        0x30
#define SYSCALL_MOUNT        0x31
#define SYSCALL_UMOUNT       0x32
#define SYSCALL_FCNTL        0x33
#define SYSCALL_TIME         0x34
#define SYSCALL_SETPGID      0x35
#define SYSCALL_GETPGRP      0x36
#define SYSCALL_FORK_PGRP    0x37
#define SYSCALL_IOCTL        0x38
#define SYSCALL_POLL         0x39
#define SYSCALL_SETSID       0x3A
#define SYSCALL_GETSID       0x3B
#define SYSCALL_SOCKET       0x3C
#define SYSCALL_SENDTO       0x3D
#define SYSCALL_RECVFROM     0x3E
#define SYSCALL_BIND         0x3F
#define SYSCALL_SETSOCKOPT   0x40
#define SYSCALL_GETSOCKOPT   0x41
#define SYSCALL_GETSOCKNAME  0x42
#define SYSCALL_SHUTDOWN     0x43
#define SYSCALL_SEND         0x44
#define SYSCALL_RECV         0x45
#define SYSCALL_GETTIMEOFDAY 0x46
#define SYSCALL_SETUID       0x47
#define SYSCALL_SETREUID     0x48
#define SYSCALL_SETRESUID    0x49
#define SYSCALL_GETUID       0x4A
#define SYSCALL_GETEUID      0x4B
#define SYSCALL_SETGID       0x4C
#define SYSCALL_SETREGID     0x4D
#define SYSCALL_SETRESGID    0x4E
#define SYSCALL_GETGID       0x4F
#define SYSCALL_GETEGID      0x50
#define SYSCALL_SETGROUPS    0x51
#define SYSCALL_GETGROUPS    0x52
#define SYSCALL_UNAME        0x53

#ifdef __KERNEL__
#ifndef ASM

#include <protura/irq.h>

void syscall_init(void);

#endif
#endif

#endif
