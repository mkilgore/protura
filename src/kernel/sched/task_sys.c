/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/snprintf.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/user_check.h>
#include <protura/dump_mem.h>
#include <protura/scheduler.h>
#include <protura/task.h>
#include <protura/mm/palloc.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/fs.h>
#include <protura/wait.h>
#include <protura/signal.h>


pid_t __fork(struct task *current, pid_t pgrp)
{
    struct task *new;
    new = task_fork(current);

    if (new)
        kp(KP_TRACE, "New task: %d\n", new->pid);
    else
        kp(KP_TRACE, "Fork failed\n");

    if (new) {
        if (pgrp == 0)
            new->pgid = new->pid;
        else if (pgrp > 0)
            new->pgid = pgrp;

        kp(KP_TRACE, "Task %s: Locking list of children\n", current->name);
        using_spinlock(&current->children_list_lock)
            list_add(&current->task_children, &new->task_sibling_list);
        kp(KP_TRACE, "Task %s: Unlocking list of children: %d\n", current->name, list_empty(&current->task_children));

        irq_frame_set_syscall_ret(new->context.frame, 0);
        scheduler_task_add(new);
    }

    if (new)
        return new->pid;
    else
        return -1;
}

pid_t sys_fork(void)
{
    struct task *t = cpu_get_local()->current;
    return __fork(t, -1);
}

pid_t sys_fork_pgrp(pid_t pgrp)
{
    struct task *t = cpu_get_local()->current;
    return __fork(t, pgrp);
}

pid_t sys_getpid(void)
{
    struct task *t = cpu_get_local()->current;
    return t->pid;
}

pid_t sys_getppid(void)
{
    struct task *t = cpu_get_local()->current;
    if (t->parent)
        return t->parent->pid;
    else
        return -1;
}

void sys_exit(int code)
{
    struct task *t = cpu_get_local()->current;
    t->ret_code = code;

    kp(KP_TRACE, "Exit! %s(%p, %d):%d\n", t->name, t, t->pid, code);

    /* At this point, we have to disable interrupts to ensure we don't get
     * rescheduled. We're deleting the majority of our task information, and a
     * reschedule back to us may not work after that. */
    irq_disable();

    if (t->pid == 1)
        panic("PID 1 exited!\n");

    /* We're going to delete our address-space, including our page-table, so we
     * need to switch to the kernel's to ensure we don't attempt to keep using
     * the deleted page-table. */
    arch_address_space_switch_to_kernel();

    task_make_zombie(t);

    /* Compiler barrier guarentee's that t->parent will not be read *before*
     * t->state is set to TASK_ZOMBIE. See details in task_make_zombie(). */
    barrier();

    if (t->parent)
        scheduler_task_wake(t->parent);

    /* If we're a kernel process, we will never be wait()'d on, so we mark
     * ourselves dead */
    if (flag_test(&t->flags, TASK_FLAG_KERNEL))
        scheduler_task_mark_dead(t);

    scheduler_task_yield();
    panic("scheduler_task_yield() returned after sys_exit()!!!\n");
}

pid_t sys_wait(struct user_buffer ret)
{
    return sys_waitpid(-1, ret, 0);
}

pid_t sys_waitpid(pid_t childpid, struct user_buffer wstatus, int options)
{
    int have_child = 0, have_no_children = 0;
    int kill_child = 0;
    struct task *child = NULL;
    struct task *t = cpu_get_local()->current;
    int ret_status = 0;
    pid_t child_pid = -1;

    /* We enter a 'sleep loop' here. We sleep until we're woke-up directly,
     * which is fine because we're waiting for any children to call sys_exit(),
     * which will wake us up. */
  sleep_again:
    sleep_intr {
        kp(KP_TRACE, "Task %s: Locking child list for wait4\n", t->name);
        using_spinlock(&t->children_list_lock) {
            if (list_empty(&t->task_children)) {
                have_no_children = 1;
                break;
            }

            list_foreach_entry(&t->task_children, child, task_sibling_list) {
                kp(KP_TRACE, "Checking child %s(%p, %d)\n", child->name, child, child->pid);
                if (childpid == 0) {
                    if (child->pgid != t->pgid)
                        continue;
                } else if (childpid > 0) {
                    if (child->pid != childpid)
                        continue;
                } else if (childpid < -1) {
                    if (child->pgid != -childpid)
                        continue;
                }

                if (child->state == TASK_ZOMBIE) {
                    list_del(&child->task_sibling_list);
                    kp(KP_TRACE, "Found zombie child: %d\n", child->pid);

                    if (child->ret_signal)
                        ret_status = WSIGNALED_MAKE(child->ret_signal);
                    else
                        ret_status = WEXIT_MAKE(child->ret_code);

                    kill_child = 1;
                    have_child = 1;
                    child_pid = child->pid;
                    break;
                }

                if (!(options & WUNTRACED) && !(options & WCONTINUED))
                    continue;

                kp(KP_TRACE,"Checking %d for continue.\n", child->pid);
                if ((child->ret_signal & TASK_SIGNAL_CONT) && (options & WCONTINUED)) {
                    kp(KP_TRACE, "Found continued child: %d\n", child->pid);
                    ret_status = WCONTINUED_MAKE();

                    child->ret_signal = 0;
                    have_child = 1;
                    child_pid = child->pid;
                    break;
                }

                kp(KP_TRACE,"Checking %d for stop.\n", child->pid);
                if ((child->ret_signal & TASK_SIGNAL_STOP) && (options & WUNTRACED)){
                    int status = child->ret_signal & ~TASK_SIGNAL_STOP;

                    kp(KP_TRACE, "Found stopped child: %d\n", child->pid);
                    ret_status = WSTOPPED_MAKE(status);

                    child->ret_signal = 0;
                    have_child = 1;
                    child_pid = child->pid;
                    break;
                }
            }
        }
        kp(KP_TRACE, "Task %s: Unlocking child list for wait4\n", t->name);

        if (!have_no_children && !have_child && !(options & WNOHANG)) {
            scheduler_task_yield();
            if (has_pending_signal(t))
                return -ERESTARTSYS;
            goto sleep_again;
        }
    }

    if (!have_child && options & WNOHANG)
        return 0;

    if (!have_child)
        return -ECHILD;

    if (kill_child)
        scheduler_task_mark_dead(child);

    if (!user_buffer_is_null(wstatus)) {
        int ret = user_copy_from_kernel(wstatus, ret_status);
        if (ret)
            return ret;
    }

    return child_pid;
}

int sys_dup(int oldfd)
{
    struct task *current = cpu_get_local()->current;
    struct file *filp = fd_get(oldfd);
    int newfd;
    int ret;

    ret = fd_get_checked(oldfd, &filp);
    if (ret)
        return ret;

    newfd = fd_get_empty();

    fd_assign(newfd, file_dup(filp));

    FD_CLR(newfd, &current->close_on_exec);

    return newfd;
}

int sys_dup2(int oldfd, int newfd)
{
    struct task *current = cpu_get_local()->current;
    struct file *old_filp;
    struct file *new_filp;
    int ret;

    ret = fd_get_checked(oldfd, &old_filp);
    if (ret)
        return ret;

    if (newfd > NOFILE || newfd < 0)
        return -EBADF;

    new_filp = fd_get(newfd);

    if (new_filp)
        vfs_close(new_filp);

    fd_assign(newfd, file_dup(old_filp));

    FD_CLR(newfd, &current->close_on_exec);

    return newfd;
}

int sys_setpgid(pid_t pid, pid_t pgid)
{
    struct task *current = cpu_get_local()->current;

    if (pid) {
        struct task *t = scheduler_task_get(pid);

        if (!t)
            return -ESRCH;

        if (!pgid)
            pgid = pid;

        t->pgid = pgid;

        scheduler_task_put(t);
        return 0;
    }

    if (!pgid)
        pgid = current->pid;

    current->pgid = pgid;

    return 0;
}

int sys_getpgrp(struct user_buffer pgrp)
{
    struct task *current = cpu_get_local()->current;

    return user_copy_from_kernel(pgrp, current->pgid);
}

pid_t sys_setsid(void)
{
    struct task *current = cpu_get_local()->current;

    kp(KP_TRACE, "%d: setsid\n", current->pid);
    kp(KP_TRACE, "%d: pgrp: %d, session_leader: %d\n", current->pid, current->pgid, flag_test(&current->flags, TASK_FLAG_SESSION_LEADER));

    if (flag_test(&current->flags, TASK_FLAG_SESSION_LEADER)
        || current->pid == current->pgid)
        return -EPERM;

    kp(KP_TRACE, "Setting setsid...\n");

    flag_set(&current->flags, TASK_FLAG_SESSION_LEADER);
    current->session_id = current->pgid = current->pid;
    current->tty = NULL;

    return current->pid;
}

pid_t sys_getsid(pid_t pid)
{
    struct task *current = cpu_get_local()->current;
    struct task *t;
    int ret;

    kp(KP_TRACE, "%d: getsid: %d\n", current->pid, pid);

    if (pid == 0)
        return current->session_id;

    if (pid < 0)
        return -ESRCH;

    t = scheduler_task_get(pid);

    if (t->session_id != current->session_id)
        ret = -EPERM;
    else
        ret = t->session_id;

    scheduler_task_put(t);

    return ret;
}

