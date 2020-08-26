/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/stdarg.h>
#include <protura/debug.h>
#include <protura/snprintf.h>
#include <protura/spinlock.h>
#include <protura/drivers/console.h>

#include <arch/backtrace.h>
#include <arch/reset.h>
#include <arch/asm.h>

static spinlock_t kprintf_lock = SPINLOCK_INIT();
static list_head_t kp_output_list = LIST_HEAD_INIT(kp_output_list);

static int max_log_level = CONFIG_KERNEL_LOG_LEVEL;

void kp_output_register(struct kp_output *output)
{
    flag_clear(&output->flags, KP_OUTPUT_DEAD);

    using_spinlock(&kprintf_lock) {
        if (!list_node_is_in_list(&output->node))
            list_add_tail(&kp_output_list, &output->node);
    }
}

void kp_output_unregister(struct kp_output *rm_output)
{
    struct kp_output *drop = NULL;

    using_spinlock(&kprintf_lock) {
        flag_set(&rm_output->flags, KP_OUTPUT_DEAD);

        if (rm_output->refs == 0) {
            list_del(&rm_output->node);
            drop = rm_output;
        }
    }

    if (drop && drop->ops->put)
        (drop->ops->put) (drop);
}

static void kp_output_logline(int level, const char *line)
{
    struct kp_output *output;
    struct kp_output *tmp;
    list_head_t drop_list = LIST_HEAD_INIT(drop_list);

    using_spinlock(&kprintf_lock) {
        /*
         * This iteration is safe even though we drop the lock, because taking
         * a ref ensures the output won't be removed while we're writing to it
         */
        list_foreach_entry_safe(&kp_output_list, output, tmp, node) {
            if (flag_test(&output->flags, KP_OUTPUT_DEAD))
                continue;

            if (level != KP_ERROR && READ_ONCE(output->max_level) < level)
                continue;

            /* Ensure this output doesn't get dropped while we're using it */
            output->refs++;

            not_using_spinlock(&kprintf_lock)
                (output->ops->print) (output, line);

            output->refs--;

            if (unlikely(flag_test(&output->flags, KP_OUTPUT_DEAD) && output->refs == 0)) {
                list_del(&output->node);
                list_add(&drop_list, &output->node);
            }
        }
    }

    if (unlikely(!list_empty(&drop_list))) {
        list_foreach_entry_safe(&drop_list, output, tmp, node) {
            if (output->ops->put)
                (output->ops->put) (output);
        }
    }
}

static const char *level_to_str[] = {
    [KP_TRACE] = "[T]",
    [KP_DEBUG] = "[D]",
    [KP_NORMAL] = "[N]",
    [KP_WARNING] = "[W]",
    [KP_ERROR] = "[E]",
};

void kpv(int level, const char *fmt, va_list lst)
{
    /* Max length of a kp line is 128 characters */
    char kp_buf[128];

    if (level > READ_ONCE(max_log_level))
        return;

    uint32_t kernel_time_ms = protura_uptime_get_ms();
    const char *prefix = "[!]";

    if (level >= 0 && level < 5)
        prefix = level_to_str[level];

    size_t prefix_len = snprintf(kp_buf, sizeof(kp_buf), "[%d.%03d]%s: ", kernel_time_ms / 1000, kernel_time_ms % 1000, prefix);

    snprintfv(kp_buf + prefix_len, sizeof(kp_buf) - prefix_len, fmt, lst);

    kp_output_logline(level, kp_buf);
}

void kp(int level, const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    kpv(level, fmt, lst);
    va_end(lst);
}

int reboot_on_panic = 0;

static __noreturn void __panicv_internal(const char *s, va_list lst, int trace)
{
    /* Switch VT to 0 so that the console is shown to the user */
    console_switch_vt(0);

    cli();
    kpv(KP_ERROR, s, lst);
    if (trace)
        dump_stack(KP_ERROR);

    if (reboot_on_panic)
        system_reboot();

    while (1)
        hlt();
}

__noreturn void __panicv_notrace(const char *s, va_list lst)
{
    __panicv_internal(s, lst, 0);
}

__noreturn void __panic_notrace(const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    __panicv_notrace(s, lst);
    va_end(lst);
}

__noreturn void __panicv(const char *s, va_list lst)
{
    __panicv_internal(s, lst, 1);
}

__noreturn void __panic(const char *s, ...)
{
    va_list lst;
    va_start(lst, s);
    __panicv(s, lst);
    va_end(lst);
}

