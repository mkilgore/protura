#ifndef SHELL_H
#define SHELL_H

#include "job.h"

extern char *cwd;

void shell_run_line(char *line);

struct job *shell_parse_job(char *line);

#endif
