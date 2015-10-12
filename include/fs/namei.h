/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_NAMEI_H
#define INCLUDE_FS_NAMEI_H

#include <fs/inode.h>

int namei(const char *path, struct inode **result);
int namex(const char *path, struct inode *cwd, struct inode **result);

#endif
