/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_ACCESS_H
#define INCLUDE_FS_ACCESS_H

#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/users.h>

int __check_permission(struct credentials *creds, struct inode *inode, int access);
int check_permission(struct inode *inode, int access);

#endif
