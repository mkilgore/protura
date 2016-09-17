/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/kassert.h>
#include <protura/fs/file.h>
#include <protura/task.h>

void wakeup_list_init(struct wakeup_list *list)
{
    memset(list, 0, sizeof(*list));
}

void wakeup_list_add(struct wakeup_list *list, struct task *new)
{
    int i;

    for (i = 0; i < WAKEUP_LIST_MAX_TASKS; i++) {
        if (!list->tasks[i]) {
            list->tasks[i] = new;
            return ;
        }
    }
}

void wakeup_list_remove(struct wakeup_list *list, struct task *old)
{
    int i;

    for (i = 0; i < WAKEUP_LIST_MAX_TASKS; i++) {
        if (list->tasks[i] == old) {
            list->tasks[i] = NULL;
            return ;
        }
    }
}

void wakeup_list_wakeup(struct wakeup_list *list)
{
    int i;

    for (i = 0; i < WAKEUP_LIST_MAX_TASKS; i++)
        if (list->tasks[i])
            list->tasks[i]->state = TASK_RUNNING;
}

