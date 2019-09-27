#ifndef INCLUDE_PROTURA_FS_SEQ_FILE_H
#define INCLUDE_PROTURA_FS_SEQ_FILE_H

#include <protura/types.h>
#include <protura/bits.h>
#include <protura/mutex.h>
#include <protura/fs/file.h>
#include <protura/kbuf.h>

struct seq_file_ops;

enum seq_file_flags {
    SEQ_FILE_DONE,
};

struct seq_file {
    int iter_offset;

    const struct seq_file_ops *ops;
    void *priv;

    mutex_t lock;
    flags_t flags;
    struct kbuf buf;
};

/*
 * Called in the order:
 *
 *     start
 *       render
 *       next
 *       render
 *       next
 *       ...
 *     end
 *     add-page
 *     start
 *       render
 *       next
 *       ...
 *     ...
 *
 * start: Should take any required locks and iterate to `iter_offset`
 *
 * render: Write the current entry to the seq_file
 *         Should return -ENOSPC if the buffer needs more space
 *
 * next: Iterate to the next `iter_offset`
 *       Should return -1 if the sequence is complete
 *
 * end: Release any locks/resources/etc. taken by start()
 */
struct seq_file_ops {
    int (*start) (struct seq_file *);
    void (*end) (struct seq_file *);
    int (*next) (struct seq_file *);
    int (*render) (struct seq_file *);
};

int seq_open(struct file *, const struct seq_file_ops *);
off_t seq_lseek(struct file *, off_t, int);
int seq_read(struct file *, void *, size_t);
int seq_release(struct file *);

int seq_printf(struct seq_file *, const char *, ...);

int seq_list_start(struct seq_file *, list_head_t *);
int seq_list_start_header(struct seq_file *seq, list_head_t *head);

int seq_list_next(struct seq_file *, list_head_t *);

list_node_t *seq_list_get(struct seq_file *);
#define seq_list_get_entry(seq, type, member) \
    ({ \
        void *__ret = NULL; \
        list_node_t *__next = seq_list_get((seq)); \
        if (__next) \
            __ret = container_of(__next, type, member); \
        __ret; \
    })

#endif
