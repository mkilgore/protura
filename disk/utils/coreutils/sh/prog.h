#ifndef PROG_H
#define PROG_H

#include <unistd.h>

struct prog_desc {
    char *file;

    int argc;
    char **argv;

    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
};

/* On success, the Child's PID will be in '*child_pid',
 * and prog_start returns zero. ,
 * and prog_start returns zero. */
int prog_start(const struct prog_desc *, pid_t *child_pid);

#endif
