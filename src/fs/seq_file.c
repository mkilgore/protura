/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/mm/palloc.h>
#include <protura/mm/kmalloc.h>
#include <protura/fs/file.h>
#include <protura/kbuf.h>
#include <protura/fs/seq_file.h>

static void seq_file_init(struct seq_file *seq, const struct seq_file_ops *ops)
{
    *seq = (struct seq_file) {
        .ops = ops,
        .lock = MUTEX_INIT(seq->lock, "seq-file-lock"),
        .buf = KBUF_INIT(seq->buf),
    };
}

static void seq_file_clear(struct seq_file *seq)
{
    kbuf_clear(&seq->buf);
}

/*
 * Currently, we have very simple logic that simply fills the buffer to the max
 * when the first read for this file comes in.
 *
 * Eventually, we could make the logic a bit smarter to only read up to the
 * point that the userspace buffer requires (Though that option is a bit racier
 * if userspace uses small buffers).
 */
static int __seq_file_fill(struct seq_file *seq)
{
    while (1) {
        if (flag_test(&seq->flags, SEQ_FILE_DONE))
            break;

        /* Make sure we have at least a page of free space */
        if (kbuf_get_free_length(&seq->buf) < PG_SIZE)
            kbuf_add_page(&seq->buf);

        int err = (seq->ops->start) (seq);
        if (err < 0)
            return err;

        if (flag_test(&seq->flags, SEQ_FILE_DONE))
            break;

        /* Loop until we hit the end of the sequence, or run out of space */
        while (1) {
            err = (seq->ops->render) (seq);
            if (err < 0)
                break;

            err = (seq->ops->next) (seq);
            if (err < 0)
                break;

            if (flag_test(&seq->flags, SEQ_FILE_DONE))
                break;
        }

        (seq->ops->end) (seq);

        /* On -ENOSPC, we simply loop again, which will trigger allocating
         * another page */
        if (err != -ENOSPC)
            return err;
    }

    return 0;
}

static int seq_file_read(struct seq_file *seq, off_t off, void *ptr, size_t sizet_len)
{
    if (off < 0)
        return -EINVAL;

    if (sizet_len > __OFF_MAX || (off_t)sizet_len < 0)
        return -EINVAL;

    off_t len = sizet_len;

    using_mutex(&seq->lock) {
        __seq_file_fill(seq);

        return kbuf_read(&seq->buf, off, ptr, len);
    }
}

int seq_printf(struct seq_file *seq, const char *fmt, ...)
{
    va_list lst;
    va_start(lst, fmt);
    int ret = kbuf_printfv(&seq->buf, fmt, lst);
    va_end(lst);
    return ret;
}

int seq_open(struct file *filp, const struct seq_file_ops *ops)
{
    struct seq_file *seq = kmalloc(sizeof(*seq), PAL_KERNEL);
    if (!seq)
        return -ENOMEM;

    seq_file_init(seq, ops);
    filp->priv_data = seq;

    return 0;
}

off_t seq_lseek(struct file *filp, off_t off, int whence)
{
    struct seq_file *seq = filp->priv_data;

    using_mutex(&seq->lock) {
        switch (whence) {
        case SEEK_SET:
            filp->offset = off;
            break;

        case SEEK_END:
            __seq_file_fill(seq);
            filp->offset = kbuf_get_length(&seq->buf) + off;
            break;

        case SEEK_CUR:
            filp->offset += off;
            break;

        default:
            return -EINVAL;
        }

        if (filp->offset < 0)
            filp->offset = 0;

        if (filp->offset == 0) {
            flag_clear(&seq->flags, SEQ_FILE_DONE);
            seq->iter_offset = 0;

            kbuf_clear(&seq->buf);
        }
    }


    return filp->offset;
}

int seq_read(struct file *filp, void *ptr, size_t len)
{
    struct seq_file *seq = filp->priv_data;

    if (!file_is_readable(filp))
        return -EBADF;

    int ret = seq_file_read(seq, filp->offset, ptr, len);

    if (ret > 0)
        filp->offset += ret;

    return ret;
}

int seq_release(struct file *filp)
{
    struct seq_file *seq = filp->priv_data;

    seq_file_clear(seq);

    kfree(seq);
    filp->priv_data = NULL;

    return 0;
}

static int __seq_list_iter(struct seq_file *seq, list_head_t *head, int start_offset)
{
    list_node_t *node;

    if (list_empty(head)) {
        flag_set(&seq->flags, SEQ_FILE_DONE);
        return 0;
    }

    node = __list_first(head);

    int i;
    for (i = start_offset; i < seq->iter_offset; i++) {
        if (list_is_last(head, node)) {
            flag_set(&seq->flags, SEQ_FILE_DONE);
            return 0;
        }

        node = node->next;
    }

    seq->priv = node;
    return 0;
}

int seq_list_start(struct seq_file *seq, list_head_t *head)
{
    return __seq_list_iter(seq, head, 0);
}

int seq_list_start_header(struct seq_file *seq, list_head_t *head)
{
    if (seq->iter_offset == 0) {
        seq->priv = NULL;
        return 0;
    }

    return __seq_list_iter(seq, head, 1);
}

int seq_list_next(struct seq_file *seq, list_head_t *head)
{
    seq->iter_offset++;
    list_node_t *node = seq->priv;

    if (!node) {
        if (list_empty(head)) {
            flag_set(&seq->flags, SEQ_FILE_DONE);
            return 0;
        }

        seq->priv = __list_first(head);
        return 0;
    }

    if (list_is_last(head, node))
        flag_set(&seq->flags, SEQ_FILE_DONE);
    else
        seq->priv = node->next;

    return 0;
}

list_node_t *seq_list_get(struct seq_file *seq)
{
    return seq->priv;
}

#ifdef CONFIG_KERNEL_TESTS
# include "seq_file_test.c"
#endif
