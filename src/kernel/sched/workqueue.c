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
#include <protura/mm/palloc.h>
#include <arch/spinlock.h>
#include <protura/snprintf.h>
#include <protura/task.h>
#include <protura/scheduler.h>
#include <protura/work.h>

static struct workqueue kwork = WORKQUEUE_INIT(kwork);

/*
 * `struct work` is designed such that a paticular instance of `struct work`
 * will only ever be run on one workqueue thread at a time (If running on a
 * workqueue). However, we also have to ensure that if a `struct work` is
 * schedule while already running, we don't "lose" that wake-up.
 *
 * We achieve this via the `WORK_SCHEDULED` flag and the `work_running_list.
 * `work_schedule` sets this flag even when the work is already on a list
 * (Either the `work_list` or `work_running_list`. When the work is done
 * running, if the `WORK_SCHEDULED` flag is set, then we simply requeue the
 * work at the end of the list.
 */
static int workqueue_thread(void *q)
{
    struct workqueue *queue = q;
    struct work *work = NULL;
    int clear_work = 0;

    while (1) {
        using_spinlock(&queue->lock) {
            if (work) {
                list_del(&work->work_entry);

                if (flag_test(&work->flags, WORK_SCHEDULED))
                    list_add_tail(&queue->work_list, &work->work_entry);
            }

            sleep_event_spinlock(!list_empty(&queue->work_list), &queue->lock);

            work = list_take_first(&queue->work_list, struct work, work_entry);

            clear_work = flag_test(&work->flags, WORK_ONESHOT);
            if (!clear_work)
                list_add_tail(&queue->work_running_list, &work->work_entry);

            /* This is fine, since we're about to run it anyway */
            flag_clear(&work->flags, WORK_SCHEDULED);
        }

        (work->callback) (work);

        if (clear_work)
            work = NULL;
    }

    return 0;
}

void workqueue_start_multiple(struct workqueue *queue, const char *thread_name, int thread_count)
{
    int i;
    struct page *tmp_page = palloc(0, PAL_KERNEL);

    using_spinlock(&queue->lock) {
        queue->thread_count = thread_count;

        for (i = 0; i < thread_count; i++) {
            snprintf(tmp_page->virt, PG_SIZE, "%s/%d", thread_name, i + 1);
            queue->work_threads[i] = task_kernel_new(tmp_page->virt, workqueue_thread, queue);
            scheduler_task_add(queue->work_threads[i]);
        }
    }

    pfree(tmp_page, 0);
}

void workqueue_start(struct workqueue *queue, const char *thread_name)
{
    workqueue_start_multiple(queue, thread_name, 1);
}

void workqueue_stop(struct workqueue *queue)
{
    int i;

    /* FIXME: This is obviously bugged, the threads may currently running
     * something. We need a different technique for shutting a workqueue down
     * (Though at the moment, we never destroy any workqueues). */

    using_spinlock(&queue->lock) {
        for (i = 0; i < queue->thread_count; i++) {
            scheduler_task_remove(queue->work_threads[i]);
            task_free(queue->work_threads[i]);
            queue->work_threads[i] = NULL;
        }

        queue->thread_count = 0;
        queue->wake_next_thread = 0;
    }
}

void workqueue_add_work(struct workqueue *queue, struct work *work)
{
    using_spinlock(&queue->lock) {
        flag_set(&work->flags, WORK_SCHEDULED);

        if (!list_node_is_in_list(&work->work_entry))
            list_add_tail(&queue->work_list, &work->work_entry);

        if (queue->thread_count) {
            scheduler_task_wake(queue->work_threads[queue->wake_next_thread]);

            queue->wake_next_thread = (queue->wake_next_thread + 1) % queue->thread_count;
        }
    }
}

void work_schedule(struct work *work)
{
    switch (work->type) {
    case WORK_CALLBACK:
        (work->callback) (work);
        break;

    case WORK_KWORK:
        workqueue_add_work(&kwork, work);
        break;

    case WORK_TASK:
        scheduler_task_wake(work->task);
        break;

    case WORK_WORKQUEUE:
        workqueue_add_work(work->queue, work);
        break;

    case WORK_NONE:
        break;
    }
}

static void kwork_delay_timer_callback(struct ktimer *timer)
{
    struct delay_work *work = container_of(timer, struct delay_work, timer);
    workqueue_add_work(&kwork, &work->work);
}

int kwork_delay_schedule(struct delay_work *work, int delay_ms)
{
    work->timer.callback = kwork_delay_timer_callback;
    return timer_add(&work->timer, delay_ms);
}

int kwork_delay_unschedule(struct delay_work *work)
{
    return timer_del(&work->timer);
}

void kwork_init(void)
{
    workqueue_start_multiple(&kwork, "kwork", 4);
}

