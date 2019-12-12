/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_MM_USER_CHECK_H
#define INCLUDE_MM_USER_CHECK_H

#include <protura/types.h>
#include <protura/bits.h>
#include <protura/task.h>
#include <protura/mm/palloc.h>
#include <arch/cpu.h>
#include <arch/user_check.h>

/* The user_copy* functions copy data from/to userspace. The wrapper functions
 * here implicietly use the type of the kernel pointer to determine how much to
 * copy.
 *
 * These functions may trigger a page-fault and sleep. */
#define user_copy_from_kernel(user_buffer, krnl) \
    ({ \
        typeof(krnl) ____tmp = (krnl); \
        user_memcpy_from_kernel((user_buffer), &(____tmp), sizeof(krnl)); \
    })

#define user_copy_from_kernel_indexed(user_buf, krnl, idx) \
    ({ \
        typeof(krnl) ____tmp = (krnl); \
        user_memcpy_from_kernel((user_buffer_index((user_buf), (idx) * sizeof(krnl))), &(____tmp), sizeof(krnl)); \
    })

#define user_copy_to_kernel(krnl, user_buffer) \
    user_memcpy_to_kernel((krnl), (user_buffer), sizeof(*(krnl)))

#define user_copy_to_kernel_indexed(krnl, buf, index) \
    user_memcpy_to_kernel((krnl), ((struct user_buffer) { .ptr = (buf).ptr + sizeof(*(krnl)) * (index), .is_user = (buf).is_user }), sizeof(*(krnl)))

#define user_buffer_index(buf, idx) \
    (struct user_buffer) { \
        .ptr = (buf).ptr + (idx), \
        .is_user = (buf).is_user \
    }

#define user_buffer_member(buf, typ, mem) \
    (struct user_buffer) { \
        .ptr = (buf).ptr + offsetof(typ, mem), \
        .is_user = (buf).is_user \
    }

#define user_buffer_make(p, usr) \
    (struct user_buffer) { \
        .ptr = (p), \
        .is_user = (usr), \
    }

#define user_buffer_is_null(buf) (!(buf).ptr)

/* Verifies that the `struct user_buffer`, going out to size, is all within the
 * userspace region */
int user_check_access(struct user_buffer data, size_t size);

/* These are wrapper functions specifically for copying strings. The regular
 * user_strncpy_to_kernel() both copies the string into `krnl`, and also
 * verifies that the string is all within the userspace region.
 *
 * user_strncpy_to_kernel_nocheck() does the same, but does *NOT* verify that
 * the string is within the userspace region */
int user_strncpy_to_kernel_nocheck(char *krnl, struct user_buffer data, size_t max);
int user_strncpy_to_kernel(char *krnl, struct user_buffer data, size_t max);

/* A convience for copying a path from userspace. Note the result pointer must
 * be pfree'd */
int user_alloc_string(struct user_buffer buf, char **result);

#define __cleanup_user_string __pfree_single_va

#endif
