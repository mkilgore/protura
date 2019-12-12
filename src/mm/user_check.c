/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/list.h>
#include <protura/snprintf.h>
#include <protura/task.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/memlayout.h>
#include <protura/mm/vm.h>
#include <protura/mm/user_check.h>

int user_check_access(const struct user_buffer data, size_t size)
{
    if (!data.is_user)
        return 0;

    /* NOTE: The subtract 1 is correct. If they're Ex. reading 4 bytes, size is
     * 4, but the last byte is ptr + 3 */
    return (uintptr_t)(data.ptr + size - 1) < KMEM_KBASE? 0: -EFAULT;
}

/*
 * We do a generic byte-by-byte copy, which is somewhat inefficient but doesn't
 * require any arch-specific code.
 *
 * Because strings are arbitrary lengths, we have two do the
 * user_check_access() logic as we go. To achieve this, we have two different
 * lengths to worry about:
 *
 * max_len:      Indicates the max length the kernel buffer can hold.
 * max_addr_len: Indicates the max length allowed by user_check_access()
 *
 * In most cases, max_len > max_addr_len (Assuming valid input - completely
 * invalid kernel addresses won't even reach this function). In this case, this
 * is a typical strncpy with length max_addr_len.
 *
 * In some cases, particularly strings on the stack like arguments, the strings
 * will be close to kernel memory and the max length may stretch into kernel
 * memory. In this case, max_len < max_addr_len, and if the string data
 * stretches into kernel memory, we need to return -EFAULT.
 *
 * Returns -ENAMETOOLONG on overflow, -EFAULT on bad address.
 */
static int user_strncpy_to_kernel_inner(char *krnl, const struct user_buffer data, size_t max_len)
{
    size_t i;

    for (i = 0; i < max_len; i++) {
        int ret = user_copy_to_kernel_indexed(krnl + i, data, i);
        if (ret < 0)
            return ret;

        if (!krnl[i])
            break;
    }

    if (i == max_len && *krnl)
        return -ENAMETOOLONG;

    for (; i < max_len; i++)
        krnl[i] = 0;

    return 0;
}

int user_strncpy_to_kernel_nocheck(char *krnl, const struct user_buffer data, size_t max)
{
    return user_strncpy_to_kernel_nocheck(krnl, data, max);
}

int user_strncpy_to_kernel(char *krnl, const struct user_buffer data, size_t max)
{
    size_t addr_max = 0xFFFFFFFF;

    if (data.is_user) {
        uintptr_t user_ptr = (uintptr_t)data.ptr;

        if (user_ptr >= KMEM_KBASE)
            return -EFAULT;

        addr_max = KMEM_KBASE - user_ptr;
    }

    int ret = user_strncpy_to_kernel_inner(krnl, data, (max < addr_max)? max: addr_max);

    /* If we never found the end of the string, and the address limit was lower
     * than our max, then we hit the kernel addresses and should report a fault */
    if (ret == -ENAMETOOLONG && addr_max < max)
        return -EFAULT;

    return ret;
}

int user_alloc_string(struct user_buffer buf, char **result)
{
    *result = palloc_va(0, PAL_KERNEL);
    if (!*result)
        return -ENOMEM;

    return user_strncpy_to_kernel(*result, buf, PG_SIZE);
}
