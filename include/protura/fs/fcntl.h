/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_FCNTL_H
#define INCLUDE_FS_FCNTL_H

#include <uapi/protura/fs/fcntl.h>

#define IS_RDWR(mode) (((mode) & O_RDWR) == O_RDWR)
#define IS_RDONLY(mode) (((mode) & O_RDONLY) == O_RDONLY)
#define IS_WRONLY(mode) (((mode) & O_WRONLY) == O_WRONLY)

#endif
