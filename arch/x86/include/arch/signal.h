#ifndef INCLUDE_ARCH_SIGNAL_H
#define INCLUDE_ARCH_SIGNAL_H

#ifdef __KERNEL__
# include <protura/types.h>
#endif

#define SIGHUP       1
#define SIGINT       2
#define SIGQUIT      3
#define SIGILL       4
#define SIGTRAP      5
#define SIGABRT      6
#define SIGIOT       6
#define SIGBUS       7
#define SIGFPE       8
#define SIGKILL      9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGSTKFL    16
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20
#define SIGTTIN     21
#define SIGTTOU     22
#define SIGURG      23
#define SIGXCPU     24
#define SIGXFSZ     25
#define SIGVTALR    26
#define SIGPROF     27
#define SIGWINCH    28
#define SIGIO       29

#ifdef __KERNEL__
#define SIG_BIT(x) (F((x) - 1))
#define SIG_UNBLOCKABLE (SIG_BIT(SIGKILL) | SIG_BIT(SIGSTOP))
#endif

#define NSIG        32
#define _NSIG NSIG


typedef uint32_t sigset_t;

typedef void (*sighandler_t) (int);

struct sigaction {
    sighandler_t sa_handler;
    sigset_t sa_mask;
    int sa_flags;
};

#define SIG_DFL ((void (*)(int)) 0)
#define SIG_IGN ((void (*)(int)) 1)
#define SIG_ERR ((void (*)(int)) -1)

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#endif
