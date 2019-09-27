/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_SYNC_H
#define INCLUDE_FS_SYNC_H

#include <protura/fs/super.h>

void super_sync(struct super_block *sb);
void __super_sync(struct super_block *sb);

void sys_sync(void);
int mount_root(dev_t device, const char *fsystem);

extern const struct file_ops mount_file_ops;

#endif
