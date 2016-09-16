/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_FS_PIPE_H
#define INCLUDE_PROTURA_FS_PIPE_H

#include <protura/types.h>
#include <protura/list.h>
#include <protura/wait.h>
#include <protura/mutex.h>

/* pipe_info is embedded as part of an inode, so there is no locking
 * information directly in this structure - Locking is done via the inode. */
struct pipe_info {
    /* These are offsets into the current first/last page: 'write_offset' is an
     * offset into the last page, and the 'read_offset' is an offset into the
     * first page.
     *
     * The 'write_offset' is always further then the 'read_offset' if they're
     * talking about the same page of data. */
    off_t read_offset, write_offset;

    int readers, writers;

    /* Queue is used to wait for data that can be read, or for pages to be
     * free'd for a write. */
    struct wait_queue read_queue, write_queue;

    /* 'bufs' is the current pages with actual data in them. 'free_pages' is a
     * list of the pages availiable to be used in 'bufs'.
     * 'total_pages' is the total number of pages contained in 'free_pages' and
     * 'bufs' - Used to limit the total number of pages this pipe can own at
     * any one time. */
    mutex_t pipe_buf_lock;

    int total_pages;
    list_head_t free_pages;
    list_head_t bufs;
};

#define PIPE_INFO_INIT(pipe) \
    { \
        .read_offset = 0, \
        .write_offset = 0, \
        .readers = 0, \
        .writers = 0, \
        .read_queue = WAIT_QUEUE_INIT((pipe).read_queue, "pipe-info-read"), \
        .write_queue = WAIT_QUEUE_INIT((pipe).write_queue, "pipe-info-write"), \
        .pipe_buf_lock = MUTEX_INIT((pipe).pipe_buf_lock, "pipe-info-buf-lock"), \
        .total_pages = 0, \
        .free_pages = LIST_HEAD_INIT((pipe).free_pages), \
        .bufs = LIST_HEAD_INIT((pipe).bufs), \
    }


static inline void pipe_info_init(struct pipe_info *pipe)
{
    *pipe = (struct pipe_info)PIPE_INFO_INIT(*pipe);
}

void pipe_init(void);
int inode_is_pipe(struct inode *inode);

struct file_ops;

extern struct file_ops pipe_read_file_ops;
extern struct file_ops pipe_write_file_ops;
extern struct file_ops pipe_default_file_ops;

extern struct file_ops fifo_read_file_ops;
extern struct file_ops fifo_write_file_ops;
extern struct file_ops fifo_rdwr_file_ops;
extern struct file_ops fifo_default_file_ops;

#endif
