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
#include <protura/mm/user_ptr.h>

int sys_sigprocmask(int how, const sigset_t *__user set, sigset_t *__user oldset)
{
    int ret;
    sigset_t *blocked = &cpu_get_local()->current->sig_blocked;

    if (oldset) {
        ret = user_check_region(oldset, sizeof(oldset), F(VM_MAP_WRITE));
        if (ret)
            return ret;

        *oldset = *blocked;
    }

    if (set) {
        sigset_t tmp;
        ret = user_check_region(set, sizeof(*set), F(VM_MAP_READ));
        if (ret)
            return ret;

        tmp = *set;

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

int sys_sigpending(sigset_t *__user set)
{
    int ret;
    ret = user_check_region(set, sizeof(*set), F(VM_MAP_WRITE));
    if (ret)
        return ret;

    *set = cpu_get_local()->current->sig_pending;

    return 0;
}

int sys_sigaction(int signum, const struct sigaction *__user act, struct sigaction *__user oldact)
{
    int entry = signum - 1;
    struct sigaction *action = cpu_get_local()->current->sig_actions + entry;
    int ret;

    if (signum < 1 || signum > NSIG)
        return -EINVAL;

    if (oldact) {
        ret = user_check_region(oldact, sizeof(*oldact), F(VM_MAP_WRITE));
        if (ret)
            return ret;

        *oldact = *action;
    }

    if (act) {
        struct sigaction tmp;

        ret = user_check_region(act, sizeof(*act), F(VM_MAP_READ));
        if (ret)
            return ret;


        tmp = *act;

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

int sys_sigwait(const sigset_t *__user set, int *__user sig)
{
    int ret, test;
    struct task *current = cpu_get_local()->current;
    sigset_t check, signals;

    ret = user_check_region(set, sizeof(*set), F(VM_MAP_READ));
    if (ret)
        return ret;

    check = *set;

    ret = user_check_region(sig, sizeof(*sig), F(VM_MAP_WRITE));
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

    *sig = test + 1;

    SIGSET_UNSET(&current->sig_pending, test + 1);

    return 0;
}

int sys_pause(void)
{
    sleep_intr
        scheduler_task_yield();

    return -ERESTARTNOHAND;
}

int sys_sigsuspend(const sigset_t *__user mask)
{
    int ret;
    struct task *current = cpu_get_local()->current;
    sigset_t tempmask;

    ret = user_check_region(mask, sizeof(*mask), F(VM_MAP_READ));
    if (ret)
        return ret;

    arch_context_set_return(&current->context, -EINTR);

    tempmask = current->sig_blocked;
    current->sig_blocked = *mask & ~SIG_UNBLOCKABLE;

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

