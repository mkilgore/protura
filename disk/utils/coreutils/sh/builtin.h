#ifndef BUILTIN_H
#define BUILTIN_H

#include "prog.h"

/* Returns 1 if builtin was not run */
int builtin_exec(struct prog_desc *, int *ret);

/* Returns true if builtin was created */
int try_builtin_create(struct prog_desc *prog);

int builtin_run(struct prog_desc *prog);

#endif
