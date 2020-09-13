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
#include <protura/hlist.h>
#include <protura/string.h>
#include <arch/spinlock.h>
#include <protura/atomic.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/palloc.h>
#include <protura/mm/user_check.h>
#include <protura/initcall.h>
#include <arch/task.h>

#include <protura/block/bcache.h>
#include <protura/block/bdev.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/vfs.h>
#include <protura/fs/pipe.h>

/* The first step is setting up a 'fake' super-block for anonymous pipes to
 * reside in. This is necessary for pipes to be correctly entered into the
 * inode-table, since inode_put will result in sb->ops->inode_dealloc being
 * called. */
static int pipe_inode_dealloc(struct super_block *pipe_superblock, struct inode *pipe_inode)
{
    kfree(pipe_inode);

    return 0;
}

static ino_t next_pipe_ino = 1;

static struct inode *pipe_inode_alloc(struct super_block *pipe_superblock)
{
    struct inode *inode = kzalloc(sizeof(*inode), PAL_KERNEL);

    inode_init(inode);

    return inode;
}

static void pipe_release_pages(struct pipe_info *pipe)
{
    struct page *page;

    list_foreach_take_entry(&pipe->free_pages, page, page_list_node)
        pfree(page, 0);

    list_foreach_take_entry(&pipe->bufs, page, page_list_node)
        pfree(page, 0);

    pipe->total_pages = 0;
}

static int pipe_inode_delete(struct super_block *pipe_superblock, struct inode *inode)
{
    kp(KP_TRACE, "Deleting pipe "PRinode"\n", Pinode(inode));
    pipe_release_pages(&inode->pipe_info);

    return 0;
}

static struct super_block_ops pipe_fake_super_block_ops = {
    .inode_dealloc = pipe_inode_dealloc,
    .inode_alloc = pipe_inode_alloc,
    .inode_read = NULL,
    .inode_write = NULL,
    .inode_delete = pipe_inode_delete,
    .sb_write = NULL,
    .sb_put = NULL,
};

/* This is initalized with the proper fields down in pipe_init() */
static struct super_block pipe_fake_super_block = SUPER_BLOCK_INIT(pipe_fake_super_block);

/* Indicates the read and write file-descriptor numbers in the fds[] array
 * passed to the syscall */
#define P_READ 0
#define P_WRITE 1

static struct inode *new_pipe_inode(void)
{
    /* struct inode *inode = pipe_inode_alloc(&pipe_fake_super_block); */
    struct inode *inode = pipe_fake_super_block.ops->inode_alloc(&pipe_fake_super_block);

    inode->ino = next_pipe_ino++;
    inode->sb = &pipe_fake_super_block;

    return inode;
}

int inode_is_pipe(struct inode *inode)
{
    return inode->sb == &pipe_fake_super_block;
}

/*
 * When a file is released, we decrease the number of readers or writers of the
 * coresponding pipe. When either number drops to zero, then we wake up the
 * coresonding wait_queue so that they can exit with an error or EOF
 *
 * IE. If a reader is waiting for data, and all the writers close, then when
 * the last writer closes we hit this code, which wakes up the remaining
 * readers. Those readers check if writers == 0, and if it does then the read
 * exits and returns zero.
 */
static int pipe_release(struct file *filp, int reader, int writer)
{
    struct inode *inode = filp->inode;
    struct pipe_info *pinfo = &inode->pipe_info;

    using_mutex(&pinfo->pipe_buf_lock) {
        if (reader) {
            pinfo->readers--;
            if (pinfo->readers == 0)
                wait_queue_wake(&pinfo->write_queue);
        }

        if (writer) {
            pinfo->writers--;
            if (pinfo->writers == 0)
                wait_queue_wake(&pinfo->read_queue);
        }

    }

    return 0;
}

static int pipe_read_release(struct file *filp)
{
    return pipe_release(filp, 1, 0);
}

static int pipe_write_release(struct file *filp)
{
    return pipe_release(filp, 0, 1);
}

static int pipe_rdwr_release(struct file *filp)
{
    return pipe_release(filp, 1, 1);
}

static int pipe_read(struct file *filp, struct user_buffer data, size_t size)
{
    size_t original_size = size;
    struct pipe_info *pinfo = &filp->inode->pipe_info;
    struct page *p;
    int wake_writers = 0;
    int ret = 0;

    if (!size)
        return 0;

    using_mutex(&pinfo->pipe_buf_lock) {
        while (size == original_size) {

            list_foreach_take_entry(&pinfo->bufs, p, page_list_node) {
                size_t cpysize = (p->lenb > size)? size: p->lenb;

                int ret = user_memcpy_from_kernel(data, p->virt + p->startb, cpysize);
                if (ret)
                    return ret;

                data = user_buffer_index(data, cpysize);
                size -= cpysize;

                p->startb += cpysize;
                p->lenb -= cpysize;

                /* check if there is still data left
                 *
                 * Adding to pinfo->bufs is ok, because if p->lenb is not zero,
                 * that means we're definitely done reading and size == 0 */
                if (p->lenb) {
                    list_add(&pinfo->bufs, &p->page_list_node);
                } else {
                    list_add(&pinfo->free_pages, &p->page_list_node);
                    wake_writers = 1;
                }

                if (size == 0)
                    break;
            }

            /* If there are no writers, then we just break early without
             * sleeping, returning anything we have. In the case of EOF, we
             * will end-up reading no data and returning zero, the intended
             * behavior. */
            if (pinfo->writers == 0)
                break;

            if (flag_test(&filp->flags, FILE_NONBLOCK)) {
                if (size == original_size)
                    ret = -EAGAIN;

                break;
            }

            if (size == original_size) {
                /* If we freed a page by reading data, then wake-up any writers
                 * that may have been waiting. */
                if (wake_writers)
                    wait_queue_wake(&pinfo->write_queue);

                wake_writers = 0;

                /* Sleep until more data */
                ret = wait_queue_event_intr_mutex(&pinfo->read_queue, !list_empty(&pinfo->bufs), &pinfo->pipe_buf_lock);
                if (ret)
                    return ret;
            }
        }

        /* If the buffer isn't empty, then wake next reader */
        if (!list_empty(&pinfo->bufs) || pinfo->writers == 0)
            wait_queue_wake(&pinfo->read_queue);

        if (wake_writers)
            wait_queue_wake(&pinfo->write_queue);
    }

    if (!ret)
        return original_size - size;
    else
        return ret;
}

static int pipe_write(struct file *filp, struct user_buffer data, size_t size)
{
    struct pipe_info *pinfo = &filp->inode->pipe_info;
    struct page *p;
    struct task *current = cpu_get_local()->current;
    size_t original_size = size;
    int wake_readers = 0;
    int ret = 0;

    if (!size)
        return 0;

    using_mutex(&pinfo->pipe_buf_lock) {
        while (size) {
            if (!pinfo->readers) {
                SIGSET_SET(&current->sig_pending, SIGPIPE);
                ret = -EPIPE;
                break;
            }

            list_foreach_take_entry(&pinfo->free_pages, p, page_list_node) {
                size_t cpysize = (PG_SIZE > size)? size: PG_SIZE;

                int ret = user_memcpy_to_kernel(p->virt, data, cpysize);
                if (ret)
                    return ret;

                p->startb = 0;
                p->lenb = cpysize;

                data = user_buffer_index(data, cpysize);
                size -= cpysize;

                list_add_tail(&pinfo->bufs, &p->page_list_node);

                wake_readers = 1;

                if (size == 0)
                    break;
            }

            /* This allows us to add more pages to our buffer if we're still under the limit.
             * If we still need more page and are at our limit for our buffer
             * size, then we may need to sleep */
            if (size && pinfo->total_pages < CONFIG_PIPE_MAX_PAGES) {
                size_t max_pages = (size / PG_SIZE + 1 > (CONFIG_PIPE_MAX_PAGES - pinfo->total_pages))
                                   ? CONFIG_PIPE_MAX_PAGES - pinfo->total_pages
                                   : size / PG_SIZE + 1;

                pinfo->total_pages += max_pages;

                for (; max_pages; max_pages--)
                    list_add(&pinfo->free_pages, &palloc(0, PAL_KERNEL)->page_list_node);

                continue;
            }

            if (flag_test(&filp->flags, FILE_NONBLOCK)) {
                if (size == original_size)
                    ret = -EAGAIN;

                break;
            }

            /* If there's no data and no readers, then we send a SIGPIPE to
             * ourselves and exit with -EPIPE */
            if (size) {
                if (wake_readers)
                    wait_queue_wake(&pinfo->read_queue);

                wake_readers = 0;

                ret = wait_queue_event_intr_mutex(&pinfo->write_queue, !list_empty(&pinfo->free_pages), &pinfo->pipe_buf_lock);
                if (ret)
                    return ret;
            }
        }

        if (!list_empty(&pinfo->free_pages) || pinfo->readers == 0)
            wait_queue_wake(&pinfo->write_queue);

        if (wake_readers)
            wait_queue_wake(&pinfo->read_queue);
    }

    if (!ret)
        return original_size - size;
    else
        return ret;
}

static int pipe_poll(struct file *filp, struct poll_table *table, int events)
{
    struct pipe_info *pinfo = &filp->inode->pipe_info;
    int ret = 0;

    using_mutex(&pinfo->pipe_buf_lock) {
        if (flag_test(&filp->flags, FILE_READABLE) && events & POLLIN) {
            if (!list_empty(&pinfo->bufs))
                ret |= POLLIN;
            else if (!pinfo->writers)
                ret |= POLLIN | POLLHUP;

            poll_table_add(table, &pinfo->read_queue);
        }

        if (flag_test(&filp->flags, FILE_WRITABLE) && events & POLLOUT) {
            if (!list_empty(&pinfo->free_pages) || pinfo->total_pages < CONFIG_PIPE_MAX_PAGES)
                ret |= POLLOUT;
            else if (!pinfo->readers)
                ret |= POLLOUT | POLLHUP;

            poll_table_add(table, &pinfo->write_queue);
        }
    }

    return ret;
}

static int fifo_open(struct inode *inode, struct file *filp)
{
    struct pipe_info *pinfo = &filp->inode->pipe_info;
    int ret = 0;

    if (flag_test(&filp->flags, FILE_READABLE) && flag_test(&filp->flags, FILE_WRITABLE)) {
        using_mutex(&pinfo->pipe_buf_lock) {
            pinfo->readers++;
            pinfo->writers++;

            wait_queue_wake(&pinfo->read_queue);
            wait_queue_wake(&pinfo->write_queue);
        }

        filp->ops = &fifo_rdwr_file_ops;
    } else if (flag_test(&filp->flags, FILE_READABLE)) {
        /* Wait for any readers on the associated pipe */
        using_mutex(&pinfo->pipe_buf_lock) {
            if (!(pinfo->readers++))
                wait_queue_wake(&pinfo->write_queue);

            /* Opening a non-blocking fifo for read always succeeds */
            if (!flag_test(&filp->flags, FILE_NONBLOCK))
                ret = wait_queue_event_intr_mutex(&pinfo->read_queue, pinfo->writers, &pinfo->pipe_buf_lock);

            if (ret == 0)
                wait_queue_wake(&pinfo->read_queue);
        }

        if (!ret)
            filp->ops = &fifo_read_file_ops;

    } else if (flag_test(&filp->flags, FILE_WRITABLE)) {
        /* Wait for any readers on the associated pipe */
        using_mutex(&pinfo->pipe_buf_lock) {

            /* Opening a non-blocking fifo fails if there are no readers */
            if (!pinfo->readers && flag_test(&filp->flags, FILE_NONBLOCK)) {
                ret = -ENXIO;
                break;
            }

            if (!(pinfo->writers++))
                wait_queue_wake(&pinfo->read_queue);

            ret = wait_queue_event_intr_mutex(&pinfo->write_queue, pinfo->readers, &pinfo->pipe_buf_lock);

            if (ret == 0)
                wait_queue_wake(&pinfo->write_queue);
        }

        if (!ret)
            filp->ops = &fifo_write_file_ops;
    } else {
        ret = -EINVAL;
    }


    return ret;
}

struct file_ops pipe_read_file_ops = {
    .open = NULL,
    .release = pipe_read_release,
    .read = pipe_read,
    .readdir = NULL,
    .read_dent = NULL,
    .lseek = NULL,
    .write = NULL,
    .poll = pipe_poll,
};

struct file_ops pipe_write_file_ops = {
    .open = NULL,
    .release = pipe_write_release,
    .read = NULL,
    .readdir = NULL,
    .read_dent = NULL,
    .lseek = NULL,
    .write = pipe_write,
};

struct file_ops fifo_read_file_ops = {
    .read = pipe_read,
    .poll = pipe_poll,
    .release = pipe_read_release,
};

struct file_ops fifo_write_file_ops = {
    .write = pipe_write,
    .poll = pipe_poll,
    .release = pipe_write_release,
};

struct file_ops fifo_rdwr_file_ops = {
    .write = pipe_write,
    .read = pipe_read,
    .poll = pipe_poll,
    .release = pipe_rdwr_release,
};

struct file_ops pipe_default_file_ops = {

};

struct file_ops fifo_default_file_ops = {
    .open = fifo_open,
};

int sys_pipe(struct user_buffer fds)
{
    int fd_local[2];
    int ret = 0;
    struct file *filps[2];
    struct inode *inode;
    struct task *current = cpu_get_local()->current;

    inode = new_pipe_inode();

    if (!inode) {
        ret = -ENFILE;
        goto ret;
    }

    kp(KP_NORMAL, "PIPE: Inode %p: "PRinode"\n", inode, Pinode(inode));

    filps[0] = kzalloc(sizeof(struct file), PAL_KERNEL);
    filps[1] = kzalloc(sizeof(struct file), PAL_KERNEL);

    filps[P_READ]->inode = inode_dup(inode);
    filps[P_READ]->flags = F(FILE_READABLE);
    filps[P_READ]->ops = &pipe_read_file_ops;
    atomic_inc(&filps[P_READ]->ref);
    inode->pipe_info.readers++;

    filps[P_WRITE]->inode = inode_dup(inode);
    filps[P_WRITE]->flags = F(FILE_WRITABLE);
    filps[P_WRITE]->ops = &pipe_write_file_ops;
    atomic_inc(&filps[P_WRITE]->ref);
    inode->pipe_info.writers++;

    fd_local[0] = fd_assign_empty(filps[0]);

    if (fd_local[0] == -1) {
        ret = -ENFILE;
        goto release_filps;
    }

    fd_local[1] = fd_assign_empty(filps[1]);

    if (fd_local[1] == -1) {
        ret = -ENFILE;
        goto release_fd_0;
    }

    FD_CLR(fd_local[P_READ], &current->close_on_exec);
    FD_CLR(fd_local[P_WRITE], &current->close_on_exec);

    ret = user_memcpy_from_kernel(fds, fd_local, sizeof(fd_local));
    if (ret)
        goto release_fd_1;

    return 0;

  release_fd_1:
    fd_release(fd_local[1]);
  release_fd_0:
    fd_release(fd_local[0]);
  release_filps:
    kfree(filps[0]);
    kfree(filps[1]);

    inode_dup(inode);
    inode_put(inode);
  ret:
    return ret;
}

static void pipe_init(void)
{
    dev_t dev = block_dev_anon_get();

    pipe_fake_super_block.bdev = block_dev_get(dev);
    pipe_fake_super_block.ops = &pipe_fake_super_block_ops;

    kp(KP_NORMAL, "Pipe dev: %d\n", pipe_fake_super_block.bdev->dev);
}
initcall_device(pipe, pipe_init);

