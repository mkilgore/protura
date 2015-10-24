/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/stddef.h>
#include <protura/string.h>
#include <arch/paging.h>

#include <init/init_task.h>

/* This is the "task" that we use during boot-up. It doesn't actually get
 * scheduled, but it contains enough information to get things working and
 * allow us to call things which require a task-context on boot-up.
 */
struct task init_task = {
    .pid = -1,
    .state = TASK_RUNNING,
    .kernel = 1,

    .page_dir = &kernel_dir,
    .parent = NULL,

    /* These fields aren't important for our uses, since the init_task isn't
     * really a 'real' task. */
    .kstack_bot = NULL,
    .kstack_top = NULL,

    .name = "INIT",
};

