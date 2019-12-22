/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_DENT_H
#define INCLUDE_FS_DENT_H

#include <protura/types.h>

struct dent {
    ino_t ino;
    uint32_t dent_len;
    uint32_t name_len;
    char name[];
};

int user_copy_dent(struct user_buffer dent, ino_t ino, uint32_t dent_len, uint32_t name_len, const char *name);

#endif
