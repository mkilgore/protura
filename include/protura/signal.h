#ifndef INCLUDE_PROTURA_SIGNAL_H
#define INCLUDE_PROTURA_SIGNAL_H

#include <arch/signal.h>

#ifdef __KERNEL__

#include <protura/mm/user_ptr.h>

int sys_sigprocmask(int how, const sigset_t *__user set, sigset_t *__user oldset);
int sys_sigpending(sigset_t *__user set);
int sys_sigaction(int signum, const struct sigaction *__user act, struct sigaction *__user oldact);
sighandler_t sys_signal(int signum, sighandler_t handler);
int sys_kill(pid_t pid, int sig);
int sys_sigwait(const sigset_t *__user set, int *__user sig);
int sys_pause(void);
int sys_sigsuspend(const sigset_t *__user mask);

#endif

#endif
