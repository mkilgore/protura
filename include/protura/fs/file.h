/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_FS_FILE_H
#define INCLUDE_FS_FILE_H

#include <protura/types.h>
#include <protura/errors.h>
#include <protura/atomic.h>
#include <protura/bits.h>
#include <protura/mutex.h>
#include <protura/fs/dirent.h>
#include <protura/fs/dent.h>
#include <protura/fs/poll.h>

struct inode;
struct file_ops;

struct file_readdir_handler {
    int (*readdir) (struct file_readdir_handler *, ino_t, mode_t, const char *name, size_t len);
};

struct file {
    struct inode *inode;
    atomic_t ref;
    mutex_t lock;
    unsigned int flags;

    off_t offset;

    const struct file_ops *ops;

    void *priv_data;
};

struct file_ops {
    int (*open) (struct inode *inode, struct file *);
    int (*release) (struct file *);
    int (*read) (struct file *, struct user_buffer, size_t);
    int (*pread) (struct file *, struct user_buffer, size_t, off_t);
    int (*readdir) (struct file *, struct file_readdir_handler *);
    int (*read_dent) (struct file *, struct user_buffer, size_t dent_size);
    off_t (*lseek) (struct file *, off_t offset, int whence);
    int (*write) (struct file *, struct user_buffer, size_t);
    int (*ioctl) (struct file *, int cmd, struct user_buffer arg);
    int (*poll) (struct file *, struct poll_table *, int events);
};

enum file_whence {
    SEEK_SET,
    SEEK_CUR,
    SEEK_END,
};

enum file_flags {
    FILE_READABLE,
    FILE_WRITABLE,
    FILE_APPEND,
    FILE_NONBLOCK,
    FILE_NOCTTY,
};

#define file_is_readable(file) (bit_test(&(file)->flags, FILE_READABLE))
#define file_is_writable(file) (bit_test(&(file)->flags, FILE_WRITABLE))
#define file_is_append(file) (bit_test(&(file)->flags, FILE_APPEND))

#define file_set_readable(file) (bit_set(&(file)->flags, FILE_READABLE))
#define file_set_writable(file) (bit_set(&(file)->flags, FILE_WRITABLE))

#define file_unset_readable(file) (bit_set(&(file)->flags, FILE_READABLE))
#define file_unset_writable(file) (bit_set(&(file)->flags, FILE_WRITABLE))

#define file_has_open(filp) ((filp)->ops && (filp)->ops->open)
#define file_has_release(filp) ((filp)->ops && (filp)->ops->release)
#define file_has_read(filp) ((filp)->ops && (filp)->ops->read)
#define file_has_pread(filp) ((filp)->ops && (filp)->ops->pread)
#define file_has_readdir(filp) ((filp)->ops && (filp)->ops->readdir)
#define file_has_read_dent(filp) ((filp)->ops && (filp)->ops->read_dent)
#define file_has_lseek(filp) ((filp)->ops && (filp)->ops->lseek)
#define file_has_write(filp) ((filp)->ops && (filp)->ops->write)

void file_init(struct file *);
void file_clear(struct file *);

struct file *file_dup(struct file *);

int fs_file_generic_pread(struct file *filp, struct user_buffer vbuf, size_t len, off_t off);
int fs_file_generic_read(struct file *, struct user_buffer buf, size_t len);
int fs_file_generic_write(struct file *, struct user_buffer buf, size_t len);
off_t fs_file_generic_lseek(struct file *, off_t off, int whence);
int fs_file_ioctl(struct file *filp, int cmd, struct user_buffer arg);

#endif
