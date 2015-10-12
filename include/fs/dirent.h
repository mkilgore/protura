/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_DIRENT_H
#define INCLUDE_FS_DIRENT_H

#include <protura/compiler.h>
#include <protura/types.h>

/* 32 bytes long, just to make it fit nicely */
struct dirent {
    char name[28];
    ino_t ino;
} __packed;

#endif
