/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_DEBUG_H
#define INCLUDE_PROTURA_DEBUG_H

#include <protura/config/autoconf.h>
#include <protura/compiler.h>
#include <protura/stdarg.h>
#include <protura/time.h>
#include <protura/list.h>
#include <arch/backtrace.h>

/* KP_STR99 is a 'catch-all' for debugging outputs which are not going to be
 * logged. */
/* Note: The KP_STR* macros need to be wrapped in Q() to get quoted */
#define KP_STR99 [!]

#define KP_STR4 [T]
#define KP_STR3 [D]
#define KP_STR2 [N]
#define KP_STR1 [W]
#define KP_STR0 [E]

#define KP_TRACE 4
#define KP_DEBUG 3
#define KP_NORMAL 2
#define KP_WARNING 1
#define KP_ERROR 0

#ifdef CONFIG_KERNEL_LOG_LOCKING
# define KP_LOCK KP_DEBUG
#else
# define KP_LOCK 99
#endif

#ifdef CONFIG_KERNEL_LOG_SPINLOCK
# define KP_SPINLOCK KP_DEBUG
#else
# define KP_SPINLOCK 99
#endif

struct kp_output {
    list_node_t node;
    void (*print) (struct kp_output *, const char *fmt, va_list lst);
    const char *name;
};

#define KP_OUTPUT_INIT(k, callback, nam) \
    { \
        .node = LIST_NODE_INIT((k).node), \
        .print = (callback), \
        .name = (nam), \
    }

void kprintf_internal(const char *s, ...) __printf(1, 2);
void kprintfv_internal(const char *s, va_list);

void kp_output_register(struct kp_output *);
void kp_output_unregister(struct kp_output *);

#ifdef CONFIG_KERNEL_LOG_SRC_LINE
# define KP_CUR_LINE Q(__LINE__) ":" __FILE__ ": "
#else
# define KP_CUR_LINE " "
#endif

static inline const char *__kp_get_loglevel_string(int level)
{
    switch (level) {
    case KP_TRACE:   return Q(TP(KP_STR, KP_TRACE));
    case KP_DEBUG:   return Q(TP(KP_STR, KP_DEBUG));
    case KP_NORMAL:  return Q(TP(KP_STR, KP_NORMAL));
    case KP_WARNING: return Q(TP(KP_STR, KP_WARNING));
    case KP_ERROR:   return Q(TP(KP_STR, KP_ERROR));
    default:         return Q(KP_STR99);
    }
}

#define kp(level, str, ...) \
    do { \
        if (level <= CONFIG_KERNEL_LOG_LEVEL) { \
            if (__builtin_constant_p((level))) \
                kprintf_internal("[%05ld]" Q(TP(KP_STR, level)) ":" KP_CUR_LINE str, protura_uptime_get(), ## __VA_ARGS__); \
            else \
                kprintf_internal("[%05ld]%s:" KP_CUR_LINE str, protura_uptime_get(), __kp_get_loglevel_string((level)), ## __VA_ARGS__); \
        } \
    } while (0)


#define panic(str, ...) __panic("[%05ld][PANIC]: " str, protura_uptime_get(), ## __VA_ARGS__);

extern int reboot_on_panic;

void __panic(const char *s, ...) __printf(1, 2) __noreturn;
void __panicv(const char *s, va_list) __noreturn;

#define BUG(str, ...) \
    do { \
        kp(KP_ERROR, "BUG: %s: %d/%s(): \"%s\"\n", __FILE__, __LINE__, __func__, str); \
        panic(__VA_ARGS__); \
    } while (0)

/* Called with 'condition' and then printf-like format string + arguments */
#define kassert(cond, ...) \
    do { \
        int __kassert_cond = (cond); \
        if (unlikely(!__kassert_cond)) \
            BUG(Q(cond), __VA_ARGS__); \
    } while (0)

#endif
