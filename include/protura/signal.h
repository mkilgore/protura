/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_SIGNAL_H
#define INCLUDE_PROTURA_SIGNAL_H

#include <uapi/protura/signal.h>

int sys_sigprocmask(int how, struct user_buffer set, struct user_buffer oldset);
int sys_sigpending(struct user_buffer set);
int sys_sigaction(int signum, struct user_buffer act, struct user_buffer oldact);
sighandler_t sys_signal(int signum, sighandler_t handler);
int sys_kill(pid_t pid, int sig);
int sys_sigwait(const struct user_buffer set, struct user_buffer sig);
int sys_pause(void);
int sys_sigsuspend(const struct user_buffer mask);

#define has_pending_signal(task) \
    ((task)->sig_pending & ~((task)->sig_blocked))

#endif
