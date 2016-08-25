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
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "input_lexer.h"
#include "builtin.h"
#include "prog.h"

static void close_prog(struct prog_desc *prog)
{
    char **arg;
    if (prog->argv)
        for (arg = prog->argv; *arg != NULL; arg++)
            free(*arg);

    free(prog->argv);

    free(prog->file);
}

static void prog_init(struct prog_desc *prog)
{
    memset(prog, 0, sizeof(*prog));
    prog->stdin_fd = STDIN_FILENO;
    prog->stdout_fd = STDOUT_FILENO;
    prog->stderr_fd = STDERR_FILENO;
}

static void prog_add_arg(struct prog_desc *prog, const char *str, size_t len)
{
    prog->argc++;
    prog->argv = realloc(prog->argv, (prog->argc + 1) * sizeof(*prog->argv));
    prog->argv[prog->argc - 1] = strndup(str, len);
    prog->argv[prog->argc] = NULL;
}

static void parse_line(const char *line)
{
    int i;
    pid_t child;
    struct prog_desc prog = { 0 };
    enum input_token token;
    struct input_lexer state = { 0 };
    char *tmp;

    state.input = line;

    prog_init(&prog);

    while ((token = lexer_next_token(&state)) != TOK_EOF) {
        enum input_token tok2;

        switch (token) {
        case TOK_COMMENT:
            while ((token = lexer_next_token(&state)),
                    token != TOK_NEWLINE && token != TOK_EOF)
                ;
            goto handle_newline;

        case TOK_STRING:
            if (!prog.file) {
                prog.file = strndup(state.str, state.len);
                prog.argc = 0;
            }
            prog_add_arg(&prog, state.str, state.len);
            break;

        case TOK_REDIRECT_OUT:
        case TOK_REDIRECT_APPEND_OUT:
        case TOK_REDIRECT_IN:
            tok2 = lexer_next_token(&state);

            if (tok2 == TOK_STRING) {
                tmp = strndup(state.str, state.len);

                if (token == TOK_REDIRECT_OUT)
                    prog.stdout_fd = open(tmp, O_WRONLY | O_CREAT, 0777);
                else if (token == TOK_REDIRECT_APPEND_OUT)
                    prog.stdout_fd = open(tmp, O_WRONLY | O_CREAT | O_APPEND, 0777);
                else
                    prog.stdin_fd = open(tmp, O_RDONLY);

                free(tmp);
            } else {
                printf("Error: Redirect requires filename\n");
                goto cleanup;
            }
            break;

        handle_newline:
        case TOK_EOF:
        case TOK_NEWLINE:
            if (!prog.file)
                goto done_exec;

            if (builtin_exec(&prog, NULL) == 0)
                goto done_exec;

            prog_start(&prog, &child);

            waitpid(child, NULL, 0);

         done_exec:
            close_prog(&prog);
            prog_init(&prog);
            break;

        case TOK_PIPE:
        {
            int pipefd[2];
            pipe(pipefd);

            prog.stdout_fd = pipefd[1];
            prog_start(&prog, &child);

            close_prog(&prog);
            prog_init(&prog);
            prog.stdin_fd = pipefd[0];
        }
            break;

        default:
            break;
        }
    }

    if (prog.file) {
        if (builtin_exec(&prog, NULL) != 0) {
            prog_start(&prog, &child);

            waitpid(child, NULL, 0);
        }
    }

cleanup:
    close_prog(&prog);
    return ;
}

void shell_run_line(char *line)
{
    parse_line(line);

    return ;
}

