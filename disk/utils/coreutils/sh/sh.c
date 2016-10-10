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
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#include "input.h"
#include "arg_parser.h"

static const char *arg_str = "[Flags] [Script]";
static const char *usage_str = "Run commands read from input\n";
static const char *arg_desc_str  = "Script: Optional file containing commands to run.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(last, NULL, '\0', 0, NULL, NULL)

enum arg_index {
  ARG_EXTRA = ARG_PARSER_EXTRA,
  ARG_ERR = ARG_PARSER_ERR,
  ARG_DONE = ARG_PARSER_DONE,
#define X(enu, ...) ARG_ENUM(enu)
  XARGS
#undef X
};

static const struct arg args[] = {
#define X(...) CREATE_ARG(__VA_ARGS__)
  XARGS
#undef X
};

char *cwd;

int main(int argc, char **argv)
{
    int fd;
    enum arg_index ret;
    int is_script = 0;
    sigset_t blocked;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            fd = open(argarg, O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                perror(argarg);
                return 0;
            }
            is_script = 1;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    sigemptyset(&blocked);
    sigaddset(&blocked, SIGCHLD);

    sigprocmask(SIG_BLOCK, &blocked, NULL);

    if (!getenv("HOME"))
        cwd = strdup("/");
    else
        cwd = strdup(getenv("HOME"));

    chdir(cwd);

    if (is_script)
        script_input_loop(fd);
    else
        keyboard_input_loop();

    return 0;
}

