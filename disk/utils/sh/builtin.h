#ifndef BUILTIN_H
#define BUILTIN_H

#include "prog.h"

/* Returns 1 if builtin was not run */
int builtin_exec(struct prog_desc *, int *ret);

#endif
