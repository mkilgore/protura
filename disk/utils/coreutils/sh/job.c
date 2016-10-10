/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

// sh - shell, command line interpreter
#define UTILITY_NAME "sh"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "input.h"
#include "prog.h"
#include "job.h"

list_head_t job_list = LIST_HEAD_INIT(job_list);

struct job *job_pid_find(pid_t pid, int *job_id, struct prog_desc **p)
{
    int count = 0;
    struct job *job;
    struct prog_desc *prog;

    list_foreach_entry(&job_list, job, job_entry) {
        count++;
        list_foreach_entry(&job->prog_list, prog, prog_entry) {
            if (prog->pid == pid) {
                *p = prog;
                *job_id = count;
                return job;
            }
        }
    }

    return NULL;
}

struct job *job_pgrp_find(pid_t pgrp)
{
    struct job *job;

    list_foreach_entry(&job_list, job, job_entry)
        if (job->pgrp == pgrp)
            return job;

    return NULL;
}

void job_clear(struct job *job)
{
    struct prog_desc *prog;

    list_foreach_entry(&job->prog_list, prog, prog_entry) {
        prog_close(prog);
        free(prog);
    }

    free(job->name);
}

void job_add_prog(struct job *job, struct prog_desc *desc)
{
    list_add_tail(&job->prog_list, &desc->prog_entry);
}

int job_is_empty(struct job *job)
{
    return list_empty(&job->prog_list);
}

int job_is_simple_builtin(struct job *job)
{
    int count = 0;
    struct prog_desc *prog;

    list_foreach_entry(&job->prog_list, prog, prog_entry) {
        if (!prog->is_builtin)
            return 0;

        if (count > 0)
            return 0;

        count++;
    }

    return 1;
}

int job_builtin_run(struct job *job)
{
    struct prog_desc *prog;
    int ret;

    prog = list_first_entry(&job->prog_list, struct prog_desc, prog_entry);

    ret = (prog->builtin) (prog);

    if (prog->stdin_fd != STDIN_FILENO)
        close(prog->stdin_fd);

    if (prog->stdout_fd != STDOUT_FILENO)
        close(prog->stdout_fd);

    if (prog->stderr_fd != STDERR_FILENO)
        close(prog->stderr_fd);

    return ret;
}

void job_first_start(struct job *job)
{
    int pgrp = 0;
    struct prog_desc *prog;

    list_foreach_entry(&job->prog_list, prog, prog_entry) {
        prog->pgid = pgrp;
        if (prog_run(prog) != 0) {
            perror("prog_run");
            exit(0);
        }

        if (!pgrp)
            pgrp = prog->pid;
    }

    job->pgrp = pgrp;
    job->state = JOB_RUNNING;
}

void job_kill(struct job *job)
{
    kill(-job->pgrp, SIGKILL);
}

void job_stop(struct job *job)
{
    kill(-job->pgrp, SIGSTOP);
    job->state = JOB_STOPPED;
}

void job_start(struct job *job)
{
    kill(-job->pgrp, SIGCONT);
    job->state = JOB_RUNNING;
}

void job_add(struct job *job)
{
    list_add_tail(&job_list, &job->job_entry);
}

void job_remove(struct job *job)
{
    list_del(&job->job_entry);
}

static int job_is_exited(struct job *job)
{
    struct prog_desc *prog;

    list_foreach_entry(&job->prog_list, prog, prog_entry)
        if (prog->pid != -1)
            return 0;

    return 1;
}

static const char *job_state_to_str[] = {
    [JOB_STOPPED] = "Stopped",
    [JOB_RUNNING] = "Running",
};

int job_output_list(struct prog_desc *desc)
{
    int count = 0;
    struct job *job;

    list_foreach_entry(&job_list, job, job_entry) {
        count++;
        dprintf(desc->stdout_fd, "[%d] %s: %s\n", count, job_state_to_str[job->state], job->name);
    }

    return 0;
}

int job_fg(struct prog_desc *desc)
{
    int job_id;
    int count = 0;
    struct job *job;

    if (desc->argc < 2) {
        dprintf(desc->stdout_fd, "bg: Please provide job number\n");
        return 1;
    }

    job_id = atoi(desc->argv[1]);

    list_foreach_entry(&job_list, job, job_entry) {
        count++;
        if (count == job_id)
            break;
    }

    if (count != job_id) {
        dprintf(desc->stdout_fd, "bg: Unknown job %d\n", job_id);
        return 1;
    }

    current_job = job;

    return 0;
}

int job_bg(struct prog_desc *desc)
{
    int job_id;
    int count = 0;
    struct job *job;

    if (desc->argc < 2) {
        dprintf(desc->stdout_fd, "bg: Please provide job number\n");
        return 1;
    }

    job_id = atoi(desc->argv[1]);

    list_foreach_entry(&job_list, job, job_entry) {
        count++;
        if (count == job_id)
            break;
    }

    if (count != job_id) {
        dprintf(desc->stdout_fd, "bg: Unknown job %d\n", job_id);
        return 1;
    }

    job_start(job);

    return 0;
}

void job_update_background(void)
{
    int wstatus;

    while (1) {
        pid_t pid = waitpid(-1, &wstatus, WUNTRACED | WCONTINUED | WNOHANG);
        int job_id;
        struct prog_desc *prog;
        struct job *job = job_pid_find(pid, &job_id, &prog);

        if (pid <= 0)
            break;

        if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
            if (!job || !prog)
                continue;

            prog->pid = -1;

            if (job_is_exited(job)) {
                job_remove(job);
                job_clear(job);
                free(job);

                if (WIFEXITED(wstatus))
                    printf("[%d] Finished: %d\n", job_id, WEXITSTATUS(wstatus));
                else if (WIFSIGNALED(wstatus))
                    printf("[%d] Killed by signal %d\n", job_id, WTERMSIG(wstatus));
            }
        } else if (WIFSTOPPED(wstatus)) {
            if (job->state == JOB_STOPPED)
                continue;

            job_stop(job);
            printf("[%d] Stopped\n", job_id);
        } else if (WIFCONTINUED(wstatus)) {
            job_start(job);
        }
    }

    return ;
}

void job_make_forground(struct job *forground_job)
{
    static int inc = 0;
    int wstatus;

    job_start(forground_job);

    tcsetpgrp(STDIN_FILENO, forground_job->pgrp);

    while (1) {
        pid_t pid = waitpid(-1, &wstatus, WUNTRACED | WCONTINUED);
        int job_id;
        struct prog_desc *prog;
        struct job *job = job_pid_find(pid, &job_id, &prog);

        if (pid == -1 && errno == ECHILD) {
            /* Hmm... */
            printf("Exiting job loop, fg_job: %s, is_empty: %d\n", forground_job->name, job_is_empty(forground_job));
            return ;
        } else if (pid == -1) {
            perror("waitpid");
            return ;
        }

        if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
            prog->pid = -1;

            if (job_is_exited(job)) {
                job_remove(job);
                job_clear(job);
                free(job);

                if (forground_job == job) {
                    tcsetpgrp(STDIN_FILENO, getpid());
                    return ;
                } else {
                    if (WIFEXITED(wstatus))
                        printf("[%d] Finished: %d\n", job_id, WEXITSTATUS(wstatus));
                    else if (WIFSIGNALED(wstatus))
                        printf("[%d] Killed by signal %d\n", job_id, WTERMSIG(wstatus));
                }
            }

        } else if (WIFSTOPPED(wstatus)) {
            if (job->state == JOB_STOPPED)
                continue;

            job_stop(job);

            tcsetpgrp(STDIN_FILENO, getpid());
            printf("[%d] Stopped (%d)\n", job_id, inc++);

            if (job == forground_job)
                return ;

            tcsetpgrp(STDIN_FILENO, forground_job->pgrp);
        } else if (WIFCONTINUED(wstatus)) {
            job_start(job);
        }
    }
}

