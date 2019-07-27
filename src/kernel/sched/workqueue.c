/*
 * Copyright (C) 2017 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <arch/spinlock.h>
#include <protura/snprintf.h>
#include <protura/task.h>
#include <protura/scheduler.h>
#include <protura/work.h>

static struct workqueue kwork = WORKQUEUE_INIT(kwork);

static int workqueue_thread(void *q)
{
    struct workqueue *queue = q;
    struct work *work;

    while (1) {
        using_spinlock(&queue->lock) {
            sleep_event_spinlock(!list_empty(&queue->work_list), &queue->lock);

            work = list_take_first(&queue->work_list, struct work, work_entry);
        }

        (work->callback) (work);
    }

    return 0;
}

void workqueue_start(struct workqueue *queue, const char *thread_name)
{
    using_spinlock(&queue->lock) {
        queue->work_thread = task_kernel_new_interruptable(thread_name, workqueue_thread, queue);
        scheduler_task_add(queue->work_thread);
    }
}

void workqueue_stop(struct workqueue *queue)
{
    using_spinlock(&queue->lock) {
        scheduler_task_remove(queue->work_thread);
        task_free(queue->work_thread);
        queue->work_thread = NULL;
    }
}

void workqueue_add_work(struct workqueue *queue, struct work *work)
{
    using_spinlock(&queue->lock) {
        list_add_tail(&queue->work_list, &work->work_entry);
        if (queue->work_thread)
            scheduler_task_wake(queue->work_thread);
    }
}

void kwork_schedule(struct work *work)
{
    workqueue_add_work(&kwork, work);
}

static void kwork_delay_timer_callback(struct ktimer *timer)
{
    struct delay_work *work = container_of(timer, struct delay_work, timer);
    workqueue_add_work(&kwork, &work->work);
}

void kwork_delay_schedule(struct delay_work *work, int delay_ms)
{
    work->timer.callback = kwork_delay_timer_callback;
    timer_add(&work->timer, delay_ms);
}

int kwork_delay_unschedule(struct delay_work *work)
{
    return timer_del(&work->timer);
}

void kwork_init(void)
{
    workqueue_start(&kwork, "kwork");
}

