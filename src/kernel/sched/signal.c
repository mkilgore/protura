/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/task.h>
#include <protura/scheduler.h>
#include <protura/signal.h>
#include <protura/mm/user_check.h>

int sys_sigprocmask(int how, struct user_buffer set, struct user_buffer oldset)
{
    int ret;
    sigset_t *blocked = &cpu_get_local()->current->sig_blocked;

    if (!user_buffer_is_null(oldset)) {
        ret = user_copy_from_kernel(oldset, *blocked);
        if (ret)
            return ret;
    }

    if (!user_buffer_is_null(set)) {
        sigset_t tmp;

        ret = user_copy_to_kernel(&tmp, set);
        if (ret)
            return ret;

        /* Remove attempts to block unblockable signals */
        tmp &= ~SIG_UNBLOCKABLE;

        kp(KP_TRACE, "sigprocmask: %d, 0x%08x\n", how, tmp);

        switch (how) {
        case SIG_BLOCK:
            *blocked |= tmp;
            break;

        case SIG_UNBLOCK:
            *blocked &= ~tmp;
            break;

        case SIG_SETMASK:
            *blocked = tmp;
            break;

        default:
            return -EINVAL;
        }
    }

    return 0;
}

int sys_sigpending(struct user_buffer set)
{
    return user_copy_from_kernel(set, cpu_get_local()->current->sig_pending);
}

int sys_sigaction(int signum, struct user_buffer act, struct user_buffer oldact)
{
    int entry = signum - 1;
    struct sigaction *action = cpu_get_local()->current->sig_actions + entry;
    int ret;

    if (signum < 1 || signum > NSIG)
        return -EINVAL;

    if (!user_buffer_is_null(oldact)) {
        ret = user_copy_from_kernel(oldact, *action);
        if (ret)
            return ret;
    }

    if (!user_buffer_is_null(act)) {
        struct sigaction tmp;

        ret = user_copy_to_kernel(&tmp, act);
        if (ret)
            return ret;

        tmp.sa_mask |= SIG_BIT(signum);
        tmp.sa_mask &= ~SIG_UNBLOCKABLE;

        *action = tmp;

        kp(KP_TRACE, "signal: Adding handler %p for %d on %d\n", action->sa_handler, signum, cpu_get_local()->current->pid);
    }

    return 0;
}

sighandler_t sys_signal(int signum, sighandler_t handler)
{
    int entry = signum - 1;
    struct sigaction *action = cpu_get_local()->current->sig_actions + entry;
    sighandler_t old_handler;

    if (signum < 1 || signum > NSIG)
        return SIG_ERR;

    old_handler = action->sa_handler;

    action->sa_handler = handler;
    action->sa_mask = 0;
    action->sa_flags = 0;

    return old_handler;
}

int sys_kill(pid_t pid, int sig)
{
    if (sig == 0)
        return scheduler_task_exists(pid);

    if (sig < 1 || sig > NSIG)
        return -EINVAL;

    if (pid > 0 || pid < -1)
        return scheduler_task_send_signal(pid, sig, 0);

    return -EINVAL;
}

int sys_sigwait(struct user_buffer set, struct user_buffer sig)
{
    int ret, test;
    struct task *current = cpu_get_local()->current;
    sigset_t check, signals;

    ret = user_copy_to_kernel(&check, set);
    if (ret)
        return ret;

  sleep_again:
    sleep_intr {
        signals = current->sig_pending & check;

        if (!signals) {
            scheduler_task_yield();
            goto sleep_again;
        }
    }

    test = bit32_find_first_set(signals);

    if (test == -1) {
        /* Uhh this shouldn't happen */
        return -EINVAL;
    }

    if ((test + 1) == SIGKILL || (test + 1) == SIGSTOP)
        return -ERESTARTSYS;

    ret = user_copy_from_kernel(sig, test + 1);
    if (ret)
        return ret;

    SIGSET_UNSET(&current->sig_pending, test + 1);

    return 0;
}

int sys_pause(void)
{
    sleep_intr
        scheduler_task_yield();

    return -ERESTARTNOHAND;
}

int sys_sigsuspend(struct user_buffer umask)
{
    int ret;
    struct task *current = cpu_get_local()->current;
    sigset_t tempmask;
    sigset_t mask;

    ret = user_copy_to_kernel(&mask, umask);
    if (ret)
        return ret;

    arch_context_set_return(&current->context, -EINTR);

    tempmask = current->sig_blocked;
    current->sig_blocked = mask & ~SIG_UNBLOCKABLE;

  sleep_again:
    sleep_intr {
        if (!(current->sig_pending & ~current->sig_blocked)) {
            scheduler_task_yield();
            goto sleep_again;
        }

        /* If signal_handle returns zero, then no signal handler was executed */
        if (!signal_handle(current, current->context.frame))
            goto sleep_again;
    }

    current->sig_blocked = tempmask;

    return -EINTR;
}

