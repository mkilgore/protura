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
#include <arch/task.h>

#include <protura/fs/block.h>
#include <protura/fs/super.h>
#include <protura/fs/file.h>
#include <protura/fs/stat.h>
#include <protura/fs/inode.h>
#include <protura/fs/inode_table.h>
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
    inode->sb_dev = pipe_fake_super_block.dev;
    inode->sb = &pipe_fake_super_block;

    return inode;
}

int inode_is_pipe(struct inode *inode)
{
    return inode->sb_dev == pipe_fake_super_block.dev;
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

    kp(KP_NORMAL, "Pipe release!\n");

    using_mutex(&pinfo->pipe_buf_lock) {
        if (reader) {
            pinfo->readers--;
            kp(KP_NORMAL, "Pipe release readers: %d!\n", pinfo->readers);
            if (pinfo->readers == 0)
                wait_queue_wake(&pinfo->write_queue);
        }

        if (writer) {
            pinfo->writers--;
            kp(KP_NORMAL, "Pipe release writers: %d!\n", pinfo->writers);
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

/*
 * A simple wrapper for sleeping on a wait_queue tied to the associated pinfo.
 *
 * Normally this isn't valid (The condition is checked before calling this
 * function). However, it is valid if you're checking variables locked under
 * pipe_buf_lock - those will not change until we release the pipe_buf_lock,
 * which happens after we start sleeping.
 */
static int __pipe_sleep_on_wait_queue(struct pipe_info *pinfo, struct wait_queue *queue)
{
    sleep_intr_with_wait_queue(queue) {
        not_using_mutex(&pinfo->pipe_buf_lock) {
            scheduler_task_yield();
            if (has_pending_signal(cpu_get_local()->current)) {
                wait_queue_unregister(&cpu_get_local()->current->wait);
                return -ERESTARTSYS;
            }
        }
    }

    return 0;
}

static int pipe_read(struct file *filp, void *data, size_t size)
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

                memcpy(data, p->virt + p->startb, cpysize);

                data += cpysize;
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
                ret = __pipe_sleep_on_wait_queue(pinfo, &pinfo->read_queue);
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

static int pipe_write(struct file *filp, const void *data, size_t size)
{
    struct pipe_info *pinfo = &filp->inode->pipe_info;
    struct page *p;
    struct task *current = cpu_get_local()->current;
    size_t original_size = size;
    int wake_readers = 0;
    int ret = 0;

    if (!size)
        return 0;

    kp(KP_NORMAL, "PIPE: "PRinode" write: %d\n", Pinode(filp->inode), size);

    using_mutex(&pinfo->pipe_buf_lock) {
        kp(KP_NORMAL, "PIPE write: got Mutex!\n");
        while (size) {
            if (!pinfo->readers) {
                SIGSET_SET(&current->sig_pending, SIGPIPE);
                ret = -EPIPE;
                break;
            }

            list_foreach_take_entry(&pinfo->free_pages, p, page_list_node) {
                kp(KP_NORMAL, "PIPE: write free_pages\n");
                size_t cpysize = (PG_SIZE > size)? size: PG_SIZE;

                memcpy(p->virt, data, cpysize);

                p->startb = 0;
                p->lenb = cpysize;

                data += cpysize;
                size -= cpysize;

                list_add_tail(&pinfo->bufs, &p->page_list_node);

                wake_readers = 1;

                kp(KP_NORMAL, "PIPE: write done free_pages\n");

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

                kp(KP_NORMAL, "PIPE: allocing %d pages\n", max_pages);

                pinfo->total_pages += max_pages;

                for (; max_pages; max_pages--)
                    list_add(&pinfo->free_pages, &palloc(0, PAL_KERNEL)->page_list_node);


                kp(KP_NORMAL, "PIPE write: %d total pages\n", pinfo->total_pages);

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

                ret = __pipe_sleep_on_wait_queue(pinfo, &pinfo->write_queue);
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
            if (!pinfo->writers && !flag_test(&filp->flags, FILE_NONBLOCK))
                ret = __pipe_sleep_on_wait_queue(pinfo, &pinfo->read_queue);

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

            if (!pinfo->readers)
                ret = __pipe_sleep_on_wait_queue(pinfo, &pinfo->write_queue);

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

int sys_pipe(int *fds)
{
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

    fds[0] = fd_assign_empty(filps[0]);

    if (fds[0] == -1) {
        ret = -ENFILE;
        goto release_filps;
    }

    fds[1] = fd_assign_empty(filps[1]);

    if (fds[1] == -1) {
        ret = -ENFILE;
        goto release_fd_0;
    }

    filps[P_READ]->mode = inode->mode;
    filps[P_READ]->inode = inode_dup(inode);
    filps[P_READ]->flags = FILE_READABLE;
    filps[P_READ]->ops = &pipe_read_file_ops;
    atomic_inc(&filps[P_READ]->ref);
    inode->pipe_info.readers++;

    filps[P_WRITE]->mode = inode->mode;
    filps[P_WRITE]->inode = inode_dup(inode);
    filps[P_WRITE]->flags = FILE_WRITABLE;
    filps[P_WRITE]->ops = &pipe_write_file_ops;
    atomic_inc(&filps[P_WRITE]->ref);
    inode->pipe_info.writers++;

    fd_assign(fds[P_READ], filps[P_READ]);
    fd_assign(fds[P_WRITE], filps[P_WRITE]);

    FD_CLR(fds[P_READ], &current->close_on_exec);
    FD_CLR(fds[P_WRITE], &current->close_on_exec);

    kp(KP_NORMAL, "PIPE: 0 inode: %p, file_refs: %d\n", filps[0]->inode, atomic_get(&filps[0]->ref));
    kp(KP_NORMAL, "PIPE: 1 inode: %p, file_refs: %d\n", filps[1]->inode, atomic_get(&filps[1]->ref));

    return 0;

    fd_release(fds[1]);
  release_fd_0:
    fd_release(fds[0]);
  release_filps:
    kfree(filps[0]);
    kfree(filps[1]);

    inode_dup(inode);
    inode_put(inode);
  ret:
    return ret;
}

void pipe_init(void)
{
    pipe_fake_super_block.dev = block_dev_anon_get();
    pipe_fake_super_block.bdev = 0;
    pipe_fake_super_block.ops = &pipe_fake_super_block_ops;
}

