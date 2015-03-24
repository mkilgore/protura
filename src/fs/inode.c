/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/list.h>
#include <protura/string.h>
#include <mm/kmalloc.h>

#include <fs/inode.h>

static LIST_HEAD(inode_free_list);
static ino_t next_inode_num = 0;

#define INODE_LIST_GROW_COUNT 20

static void inode_list_grow(void)
{
    int i;
    for (i = 0; i < INODE_LIST_GROW_COUNT; i++) {
        struct inode *inode = kmalloc(sizeof(*inode), PMAL_KERNEL);
        memset(inode, 0, sizeof(*inode));
        list_add(&inode_free_list, &inode->inode_list);
    }
}

struct inode *inode_new(void)
{
    struct inode *inode;
    if (list_empty(&inode_free_list))
        inode_list_grow();

    inode = list_first(&inode_free_list, struct inode, inode_list);
    inode->ino = next_inode_num++;
    return inode;
}

void inode_free(struct inode *inode)
{
    if (inode->ops->release)
        (inode->ops->release) (inode);

    memset(inode, 0, sizeof(*inode));
    list_add(&inode_free_list, &inode->inode_list);
}

