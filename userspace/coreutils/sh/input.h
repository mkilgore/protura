#ifndef INPUT_H
#define INPUT_H

#include "job.h"

extern struct job *current_job;

void keyboard_input_loop(void);
void script_input_loop(int fd);

#endif
