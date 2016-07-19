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
#include <protura/irq.h>
#include <arch/paging.h>
#include <protura/signal.h>

/* This is the contents of the stack - keep in mind, the stack grows downward */
struct signal_context {
    /* Above here is where the return address from the signal handler will go */

    /* Argument to the signal handler */
    int signum;

    sigset_t old_mask;

    /* Below here is where the trampoline code will be placed on the stack */
    struct irq_frame frame;
};

/*
 * Restart a syscall with the same parameters
 *
 * Since syscalls do not modify the original contents of registers besides
 * `eax`, a restart is as simple as restoring the old value of `eax` to the
 * previous syscall number, and then decrementing `eip` by 2, which is the
 * length in bytes of an `int $imm8` instruction.
 *
 * This effectivly executes the original `int` instruction again.
 */
static void signal_syscall_restart(struct irq_frame *iframe, int prev_syscall)
{
    iframe->eax = prev_syscall;
    iframe->eip -= 2;
}

void sys_sigreturn(struct irq_frame *iframe)
{
    struct signal_context context;
    struct task *current = cpu_get_local()->current;
    char *stack;

    stack = (char *)iframe->esp;

    context = *(struct signal_context *)stack;

    *iframe = context.frame;
    current->sig_blocked = context.old_mask & ~SIG_UNBLOCKABLE;
}

static void signal_setup_stack(struct task *current, struct sigaction *action, int signum, struct irq_frame *iframe)
{
    struct signal_context context;
    char *stack, *signal_ret;

    context.frame = *iframe;
    context.old_mask = current->sig_blocked;
    context.signum = signum;

    stack = (char *)iframe->esp;

    kp(KP_TRACE, "iframe->esp: %p\n", stack);

    stack = ALIGN_2_DOWN(stack, 4);

    stack -= x86_trampoline_len;
    memcpy(stack, x86_trampoline_code, x86_trampoline_len);
    signal_ret = stack;

    kp(KP_TRACE, "signal_ret: %p\n", signal_ret);

    stack = ALIGN_2_DOWN(stack, 4);

    stack -= sizeof(context);
    *(struct signal_context *)stack = context;

    stack -= sizeof(signal_ret);
    *(char **)stack = signal_ret;

    iframe->esp = (uint32_t)stack;
    iframe->eip = (uint32_t)action->sa_handler;
    kp(KP_TRACE, "iframe->eip: %p\n", (void *)iframe->eip);
}

static void signal_jump(struct task *current, int signum, struct irq_frame *iframe)
{
    struct sigaction *action = current->sig_actions + signum - 1;

    if (current->context.prev_syscall) {
        switch (iframe->eax) {
        case -ERESTARTSYS:
            if (action->sa_flags & SA_RESTART)
                signal_syscall_restart(iframe, current->context.prev_syscall);
            else
                iframe->eax = -EINTR;
            break;

        case -ERESTARTNOINTR:
            signal_syscall_restart(iframe, current->context.prev_syscall);
            break;

        case -ERESTARTNOHAND:
            iframe->eax = -EINTR;
            break;
        }
    }

    signal_setup_stack(current, action, signum, iframe);

    if (action->sa_flags & SA_ONESHOT)
        action->sa_handler = NULL;

    current->sig_blocked |= action->sa_mask;
}

static void signal_default(struct task *current, int signum)
{
    /* Init ignores every signal */
    if (current->pid == 1)
        return ;

    switch (signum) {
    case SIGCHLD:
    case SIGCONT:
    case SIGWINCH:
        /* Ignore */
        break;

    case SIGSTOP:
    case SIGTSTP:
        /* Stop process */
        break;

    default:
        sys_exit(0);
    }
}

int signal_handle(struct task *current, struct irq_frame *iframe)
{
    while (current->sig_pending & (~current->sig_blocked)) {
        int signum = bit32_find_first_set((current->sig_pending & (~current->sig_blocked))) + 1;
        struct sigaction *action = current->sig_actions + signum - 1;

        kp(KP_TRACE, "signal: Handling %d on %d\n", signum, current->pid);
        kp(KP_TRACE, "signal: Handler: %p\n", action->sa_handler);

        SIGSET_UNSET(&current->sig_pending, signum);

        if (action->sa_handler == SIG_IGN) {
            if (signum == SIGCHLD) {
                while (sys_waitpid(-1, NULL, WNOHANG) > 0)
                    /* Reap chlidren */;
            }
            continue;
        } else if (action->sa_handler == SIG_DFL) {
            signal_default(current, signum);
        } else {
            signal_jump(current, signum, iframe);
            return 1;
        }
    }

    if (current->context.prev_syscall) {
        switch (iframe->eax) {
        case -ERESTARTSYS:
        case -ERESTARTNOINTR:
        case -ERESTARTNOHAND:
            signal_syscall_restart(iframe, current->context.prev_syscall);
            break;
        }
    }

    return 0;
}

