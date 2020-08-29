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
#include <protura/list.h>

#define KP_TRACE 4
#define KP_DEBUG 3
#define KP_NORMAL 2
#define KP_WARNING 1
#define KP_ERROR 0

enum {
    KP_OUTPUT_DEAD,
};

struct kp_output;

struct kp_output_ops {
    void (*print) (struct kp_output *, const char *str);
    void (*put) (struct kp_output *);
};

struct kp_output {
    list_node_t node;
    int flags;
    int refs;
    int max_level;

    const char *name;
    const struct kp_output_ops *ops;
};

#define KP_OUTPUT_INIT(k, lvl, nam, op) \
    { \
        .node = LIST_NODE_INIT((k).node), \
        .max_level = (lvl), \
        .name = (nam), \
        .ops = (op), \
    }

void kp_output_register(struct kp_output *);
void kp_output_unregister(struct kp_output *);

void kp(int level, const char *str, ...) __printf(2, 3);
void kpv(int level, const char *str, va_list lst);

/* These 'force' functions ignore the global log level and force the line to be
 * logged regardless. Individual kp_output max's still apply. */
void kp_force(int level, const char *str, ...) __printf(2, 3);
void kpv_force(int level, const char *str, va_list lst);

/* This is a convience macro for creating more specific loggers that use their
 * own max loglevel */
#define kp_check_level(lvl, var, str, ...) \
    do { \
        if (READ_ONCE((var)) >= (lvl)) \
            kp_force((lvl), str, ## __VA_ARGS__); \
    } while (0)

extern int reboot_on_panic;

void __panic(const char *s, ...) __printf(1, 2) __noreturn;
void __panicv(const char *s, va_list) __noreturn;
void __panic_notrace(const char *s, ...) __printf(1, 2) __noreturn;
void __panicv_notrace(const char *s, va_list) __noreturn;

#define panic(str, ...) __panic(str, ## __VA_ARGS__)
#define panic_notrace(str, ...) __panic_notrace(str, ## __VA_ARGS__)

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
