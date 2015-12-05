/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_DEBUG_H
#define INCLUDE_PROTURA_DEBUG_H

#include <config/autoconf.h>
#include <protura/compiler.h>
#include <protura/stdarg.h>
#include <arch/backtrace.h>

/* KP_STR99 is a 'catch-all' for debugging outputs which are not going to be
 * logged. */
#define KP_STR99 ""

#define KP_STR4 "[T]"
#define KP_STR3 "[D]"
#define KP_STR2 "[N]"
#define KP_STR1 "[W]"
#define KP_STR0 "[E]"

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

void kprintf_internal(const char *s, ...) __printf(1, 2);
void kprintfv_internal(const char *s, va_list);
void kp_output_register(void (*print) (const char *fmt, va_list lst), const char *name);
void kp_output_unregister(void (*print) (const char *fmt, va_list lst));

#define kp(level, str, ...) \
    do { \
        if (level <= CONFIG_KERNEL_LOG_LEVEL) { \
            kprintf_internal(TP(KP_STR, level) ":" Q(__LINE__) ":" __FILE__ ": " str, ## __VA_ARGS__); \
        } \
    } while (0)

#define panic(str, ...) \
    do { \
        cli(); \
        kprintf_internal("[PANIC]: " str, ## __VA_ARGS__); \
        dump_stack(); \
        while (1) \
            hlt(); \
    } while (0)

/*
void panic(const char *s, ...) __printf(1, 2) __noreturn;
void panicv(const char *s, va_list) __noreturn; */

#endif
