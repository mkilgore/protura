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
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#include "input.h"

void handle_child(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

void handle_sigint(int sig)
{
    pid_t pid = getpid();
    /* "disable" SIGINT on the main thread so we don't kill the shell when we
     * kill our children */
    signal(SIGINT, SIG_IGN);
    kill(-pid, SIGINT);
}

void ignore_sig(int sig)
{
    /* Restore SIGINT handler - We "disable" it by setting it to calling this
     * function when we use SIGINT to kill off all of our children */
    signal(SIGINT, handle_sigint);
}

int main(int argc, char **argv)
{
    setvbuf(stdin, NULL, _IONBF, 0);

    signal(SIGCHLD, handle_child);
    signal(SIGINT, handle_sigint);
    input_loop();
    return 0;
}

