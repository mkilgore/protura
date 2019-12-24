#ifndef PROG_H
#define PROG_H

#include <unistd.h>
#include "list.h"

struct prog_desc {
    char *file;

    int argc;
    char **argv;

    int stdin_fd;
    int stdout_fd;
    int stderr_fd;

    unsigned int is_builtin :1;

    int (*builtin) (struct prog_desc *);

    pid_t pgid;

    pid_t pid;

    list_node_t prog_entry;
};

#define PROG_DESC_INIT(prog) \
    { \
        .prog_entry = LIST_NODE_INIT((prog).prog_entry), \
        .stdin_fd = STDIN_FILENO, \
        .stdout_fd = STDOUT_FILENO, \
        .stderr_fd = STDERR_FILENO, \
    }

static inline void prog_desc_init(struct prog_desc *desc)
{
    *desc = (struct prog_desc)PROG_DESC_INIT(*desc);
}

/* On success, the Child's PID will be in '*child_pid',
 * and prog_start returns zero. ,
 * and prog_start returns zero. */
#if 0
int prog_start(const struct prog_desc *, pid_t *child_pid);
#endif

int prog_run(struct prog_desc *prog);
void prog_add_arg(struct prog_desc *prog, const char *str, size_t len);
void prog_close(struct prog_desc *prog);

#endif
