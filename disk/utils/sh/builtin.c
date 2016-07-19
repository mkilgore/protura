
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>

#include "builtin.h"

struct builtin_cmd {
    const char *id;
    int (*cmd) (struct prog_desc *);
};

static int cd(struct prog_desc *prog)
{
    if (prog->argc <= 1)
        return 1;

    chdir(prog->argv[1]);

    return 0;
}

static struct builtin_cmd cmds[] = {
    { .id = "cd", .cmd = cd },
    { .id = NULL, .cmd = NULL }
};

int builtin_exec(struct prog_desc *prog, int *ret)
{
    struct builtin_cmd *cmd = cmds;

    if (!prog->file)
        return 1;

    for (; cmd->id; cmd++) {
        if (strcmp(prog->file, cmd->id) == 0) {
            int k;

            k = (cmd->cmd) (prog);

            if (ret)
                *ret = k;

            return 0;
        }
    }

    return 1;
}

