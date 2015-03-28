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
#include <protura/spinlock.h>
#include <protura/string.h>
#include <mm/kmalloc.h>

#include <fs/inode.h>
#include <fs/vfsnode.h>

static struct {
    struct spinlock lock;
    struct list_head free_vfsnodes;
} vfs_list = {
    .lock = SPINLOCK_INIT("vfs table lock"),
    .free_vfsnodes = LIST_HEAD_INIT(vfs_list.free_vfsnodes),
};


struct vfsnode *vfs_root = NULL;

#define VFSNODE_LIST_GROW_COUNT 20

static void vfsnode_pool_grow(void)
{
    int i;

    using_spinlock(&vfs_list.lock) {
        for (i = 0; i < VFSNODE_LIST_GROW_COUNT; i++) {
            struct vfsnode *node = kmalloc(sizeof(*node), PMAL_KERNEL);
            memset(node, 0, sizeof(*node));
            list_add(&vfs_list.free_vfsnodes, &node->empty);
        }
    }
}

struct vfsnode *vfsnode_get_new(void)
{
    struct vfsnode *node;
    int empty;

    using_spinlock(&vfs_list.lock)
        empty = list_empty(&vfs_list.free_vfsnodes);

    if (empty)
        vfsnode_pool_grow();

    using_spinlock(&vfs_list.lock)
        node = list_take_first(&vfs_list.free_vfsnodes, struct vfsnode, empty);

    memset(node->name, 0, sizeof(node->name));
    INIT_LIST_HEAD(&node->children);

    return node;
}

void __vfsnode_setup_dots(struct vfsnode *node)
{
    struct vfsnode *dot = vfsnode_get_new();

    strcpy(dot->name, ".");
    dot->parent = node;

    list_add(&node->children, &dot->child_entry);

    dot = vfsnode_get_new();

    strcpy(dot->name, "..");
    dot->parent = node->parent;

    list_add(&node->children, &dot->child_entry);
}

void vfsnode_free(struct vfsnode *node)
{
    using_spinlock(&vfs_list.lock)
        list_add(&vfs_list.free_vfsnodes, &node->empty);
}

char *get_next(char **path)
{
    char *start;

    if (!**path)
        return NULL;

    start = *path;
    while ((*path)[0] != '/' && (*path)[0])
        (*path)++;

    if ((*path)[0] == '/') {
        (*path)[0] = '\0';
        (*path)++;
    }

    return start;
}

struct vfsnode *vfs_find(const char *path)
{
    struct vfsnode *node = vfs_root, *nnode = NULL;
    char name[256], *pos, *next;

    if (strcmp(path, "/") == 0)
        return vfs_root;

    strcpy(name, path);

    pos = name + 1;

    kprintf("Path: %s\n", path);

    while ((next = get_next(&pos))) {
        kprintf("next: %s, pos: %s\n", next, pos);
        using_spinlock(&node->lock) {
            struct vfsnode *child;
            list_foreach_entry(&node->children, child, child_entry) {
                if (strcmp(child->name, next) == 0) {
                    nnode = child;
                    break;
                }
            }
        }
        if (nnode)
            node = nnode;
    }

    return node;
}

void vfs_init(void)
{
    const char **n;
    struct vfsnode *r = vfsnode_get_new();

    /* Setup the 'root' node. Root is root's own parent */
    strcpy(r->name, "");
    r->parent = r;

    INIT_LIST_HEAD(&r->children);

    __vfsnode_setup_dots(r);

    for (n = (const char *[]) { "etc", "dev", "bin", NULL };
         *n; n++) {
        struct vfsnode *node = vfsnode_get_new();
        strcpy(node->name, *n);
        node->parent = r;
        __vfsnode_setup_dots(node);

        list_add(&r->children, &node->child_entry);
        r->child_count++;
    }


    vfs_root = r;
}

