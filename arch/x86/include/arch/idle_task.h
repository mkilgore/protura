#ifndef INCLUDE_ARCH_IDLE_TASK_H
#define INCLUDE_ARCH_IDLE_TASK_H

#include <protura/types.h>

void kernel_arch_idle_task_entry(void);

extern uint32_t kernel_arch_idle_task_size;

#endif
