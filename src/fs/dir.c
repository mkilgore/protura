/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/string.h>
#include <protura/mm/user_check.h>

#include <protura/fs/dent.h>

int user_copy_dent(struct user_buffer dent, ino_t ino, uint32_t dent_len, uint32_t name_len, const char *name)
{
    int ret = user_copy_from_kernel(user_buffer_member(dent, struct dent, ino), ino);
    if (ret)
        return ret;

    ret = user_copy_from_kernel(user_buffer_member(dent, struct dent, dent_len), dent_len);
    if (ret)
        return ret;

    ret = user_copy_from_kernel(user_buffer_member(dent, struct dent, name_len), name_len);
    if (ret)
        return ret;

    ret = user_memcpy_from_kernel(user_buffer_member(dent, struct dent, name), name, name_len);
    if (ret)
        return ret;

    return user_memset_from_kernel(user_buffer_index(user_buffer_member(dent, struct dent, name), name_len), 0, 1);
}
