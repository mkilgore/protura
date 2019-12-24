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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <protura/drivers/tty.h>

#include "input_lexer.h"
#include "builtin.h"
#include "prog.h"
#include "job.h"
#include "shell.h"

#define PROC_MAX 20

static struct job *parse_line(const char *line)
{
    struct job *job;
    struct prog_desc *prog;
    int start_in_background = 0;

    enum input_token token;
    struct input_lexer state = { 0 };
    char *tmp;

    job = malloc(sizeof(*job));
    job_init(job);

    job->name = strdup(line);

    char *temp_line = lexer_input_replace_env(line);
    if (temp_line)
        state.input = temp_line;
    else
        state.input = line;

    prog = malloc(sizeof(*prog));
    prog_desc_init(prog);

    while ((token = lexer_next_token(&state)) != TOK_EOF) {
        enum input_token tok2;

        switch (token) {
        case TOK_COMMENT:
            while ((token = lexer_next_token(&state)),
                    token != TOK_NEWLINE && token != TOK_EOF)
                ;
            goto handle_newline;

        case TOK_STRING:
            if (!prog->file) {
                prog->file = strndup(state.str, state.len);
                prog->argc = 0;
            }
            prog_add_arg(prog, state.str, state.len);
            break;

        case TOK_REDIRECT_OUT:
        case TOK_REDIRECT_APPEND_OUT:
        case TOK_REDIRECT_IN:
            tok2 = lexer_next_token(&state);

            if (tok2 == TOK_STRING) {
                tmp = strndup(state.str, state.len);

                if (token == TOK_REDIRECT_OUT)
                    prog->stdout_fd = open(tmp, O_WRONLY | O_CREAT, 0777);
                else if (token == TOK_REDIRECT_APPEND_OUT)
                    prog->stdout_fd = open(tmp, O_WRONLY | O_CREAT | O_APPEND, 0777);
                else
                    prog->stdin_fd = open(tmp, O_RDONLY);

                free(tmp);
            } else {
                printf("Error: Redirect requires filename\n");
                goto cleanup;
            }
            break;

        handle_newline:
        case TOK_EOF:
        case TOK_NEWLINE:
            if (!prog->file) {
                free(prog);
                goto done_with_job;
            }

            try_builtin_create(prog);
            job_add_prog(job, prog);

         done_with_job:
            prog = malloc(sizeof(*prog));
            prog_desc_init(prog);
            break;

        case TOK_PIPE:
        {
            int pipefd[2];
            pipe(pipefd);

            fcntl(pipefd[0], F_SETFD, fcntl(pipefd[0], F_GETFD) | FD_CLOEXEC);
            fcntl(pipefd[1], F_SETFD, fcntl(pipefd[1], F_GETFD) | FD_CLOEXEC);

            prog->stdout_fd = pipefd[1];

            try_builtin_create(prog);

            job_add_prog(job, prog);

            prog = malloc(sizeof(*prog));
            prog_desc_init(prog);
            prog->stdin_fd = pipefd[0];
        }
            break;

        case TOK_BACKGROUND:
            start_in_background = 1;
            break;

        default:
            break;
        }
    }

    if (prog->file) {
        try_builtin_create(prog);
        job_add_prog(job, prog);
    } else {
        free(prog->file);
        free(prog);
    }

    if (job_is_empty(job)) {
        job_clear(job);
        free(job);
        return NULL;
    }

    if (start_in_background) {
        job_add(job);
        job_first_start(job);
        job_start(job);
        job = NULL;
    }

    return job;

cleanup:
    free(temp_line);
    free(prog);
    job_clear(job);
    free(job);
    return NULL;
}

struct job *shell_parse_job(char *line)
{
    return parse_line(line);
}

