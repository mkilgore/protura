/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/mm/kmalloc.h>
#include <protura/mm/user_check.h>
#include <protura/dev.h>
#include <protura/fs/inode.h>
#include <protura/fs/file.h>
#include <protura/fs/seq_file.h>
#include <protura/block/bcache.h>
#include <protura/block/bdev.h>
#include <protura/block/disk.h>

#define DISK_HASH_TABLE_SIZE 256
static spinlock_t disk_hash_lock = SPINLOCK_INIT();
static list_head_t disk_list = LIST_HEAD_INIT(disk_list);
static struct hlist_head disk_cache[DISK_HASH_TABLE_SIZE];

/* 
 * FIXME: This isn't very good, but because disk span a *range* of minors, and
 * we don't know that range when doing a disk_get(), we can't include the minor
 * in the hash.
 *
 * We should probably switch from this basic hash-table to something else, but
 * this is fine for now.
 */
static int disk_hash(int major)
{
    return major % DISK_HASH_TABLE_SIZE;
}

struct disk *disk_alloc(void)
{
    struct disk *disk = kzalloc(sizeof(*disk), PAL_KERNEL);
    disk_init(disk);
    disk->refs++;

    disk_part_add(disk, &disk->whole);

    return disk;
}

struct disk_part *disk_part_alloc(void)
{
    struct disk_part *part = kzalloc(sizeof(*part), PAL_KERNEL);
    return part;
}

static void disk_free_parts(struct disk *disk)
{
    int part;

    /* Skip the first partition because it is embedded in the disk structure */
    for (part = 1; part < disk->part_count; part++)
        kfree(disk->parts[part]);
}

static void disk_free(struct disk *disk)
{
    disk_free_parts(disk);
    kfree(disk->parts);
    kfree(disk);
}

void disk_part_add(struct disk *disk, struct disk_part *part)
{
    int part_count;

    /* Not exactly ideal, but not a huge deal. We need to allocate the new array
     * while not under the spinlock, so we first read the current partition
     * count, drop the lock, and then allocate memory for the array.
     *
     * Then, we grab the lock again and check if the count is still the same.
     *   If yes, we can use the array we just allocated. 
     *   If no, free the array and try it again.
     */

    using_spinlock(&disk_hash_lock)
        part_count = disk->part_count;

  do_allocation_again:
    part_count++;

    /* Allocate new space, copy and free the old array */
    struct disk_part **parts = kzalloc(sizeof(*parts) * part_count, PAL_KERNEL);

    spinlock_acquire(&disk_hash_lock);

    if (disk->part_count != part_count - 1) {
        part_count = disk->part_count;
        spinlock_release(&disk_hash_lock);

        kfree(parts);
        goto do_allocation_again;
    }

    /* The check is for adding the first partition, when disk->parts won't exist */
    if (disk->parts) {
        if (disk->part_count > 0)
            memcpy(parts, disk->parts, sizeof(*parts) * (disk->part_count));

        kfree(disk->parts);
    }

    part->part_number = disk->part_count;
    parts[disk->part_count] = part;
    disk->parts = parts;
    disk->part_count++;

    spinlock_release(&disk_hash_lock);
}

struct disk_part *disk_part_get(struct disk *disk, int partno)
{
    if (partno < 0)
        return NULL;

    using_spinlock(&disk_hash_lock) {
        kp(KP_NORMAL, "pcount: %d, partno: %d\n", disk->part_count, partno);
        if (disk->part_count > partno && disk->parts)
            return disk->parts[partno];
        else
            return NULL;
    }
}

int disk_part_clear(struct disk *disk)
{
    using_spinlock(&disk_hash_lock) {
        if (disk->open_refs > 0)
            return -EBUSY;

        /* We don't have to do anything else because disk->parts is already has
         * the disk->whole entry in disk->parts[0] */
        disk->part_count = 1;
    }

    return 0;
}

int disk_register(struct disk *disk)
{
    int hash = disk_hash(disk->major);

    using_spinlock(&disk_hash_lock) {
        hlist_add(disk_cache + hash, &disk->hash_entry);
        list_add_tail(&disk_list, &disk->disk_entry);
        flag_set(&disk->flags, DISK_UP);
    }

    /* We quickly open and close the new disk to trigger read the partition information
     *
     * We also effectively ignore errors here */
    struct block_device *bdev = block_dev_get(DEV_MAKE(disk->major, disk->first_minor));
    if (!bdev)
        return 0;

    int ret = block_dev_open(bdev, 0);
    if (!ret)
        block_dev_close(bdev);

    block_dev_put(bdev);

    return 0;
}

void disk_unregister(struct disk *disk)
{
    using_spinlock(&disk_hash_lock)
        flag_clear(&disk->flags, DISK_UP);
}

struct disk *disk_get(dev_t dev, int *partno)
{
    int major = DEV_MAJOR(dev);
    int minor = DEV_MINOR(dev);
    int hash = disk_hash(major);
    struct disk *disk;

    using_spinlock(&disk_hash_lock) {
        hlist_foreach_entry(disk_cache + hash, disk, hash_entry) {
            if (disk->first_minor <= minor && disk->first_minor + disk->minor_count > minor) {
                if (!flag_test(&disk->flags, DISK_UP))
                    return NULL;

                *partno = minor - disk->first_minor;
                disk->refs++;
                return disk;
            }
        }
    }

    return NULL;
}

struct disk *disk_dup(struct disk *disk)
{
    using_spinlock(&disk_hash_lock)
        disk->refs++;

    return disk;
}

void disk_put(struct disk *disk)
{
    struct disk *drop = NULL;

    using_spinlock(&disk_hash_lock) {
        disk->refs--;

        if (disk->refs == 0) {
            kassert(disk->open_refs == 0, "Disk %s has open_refs>= but refs=0!!!", disk->name);
            hlist_del(&disk->hash_entry);
            list_del(&disk->disk_entry);
            drop = disk;
        }
    }

    if (drop) {
        if (drop->ops->put)
            drop->ops->put(drop);

        disk_free(drop);
    }
}

int disk_open(struct disk *disk)
{
    using_spinlock(&disk_hash_lock) {
        if (flag_test(&disk->flags, DISK_UP)) {
            disk->open_refs++;
        } else {
            return -ENXIO;
        }
    }

    return 0;
}

void disk_close(struct disk *disk)
{
    using_spinlock(&disk_hash_lock) {
        disk->open_refs--;
        kassert(disk->open_refs >= 0, "Disk %s open_refs<=0!", disk->name);
    }
}

void disk_capacity_set(struct disk *disk, sector_t sectors)
{
    using_spinlock(&disk_hash_lock)
        disk->whole.sector_count = sectors;
}

sector_t disk_capacity_get(struct disk *disk)
{
    using_spinlock(&disk_hash_lock)
        return disk->whole.sector_count;
}

static int disk_seq_start(struct seq_file *seq)
{
    spinlock_acquire(&disk_hash_lock);
    return seq_list_start(seq, &disk_list);
}

static void disk_seq_end(struct seq_file *seq)
{
    spinlock_release(&disk_hash_lock);
}

static int disk_create_name(struct seq_file *seq, struct disk *disk, struct disk_part *part)
{
    if (!part->part_number)
        return seq_printf(seq, "%s %d %d %lld %lld\n", disk->name, disk->major, disk->first_minor, (uint64_t)part->first_sector << disk->min_block_size_shift, (uint64_t)part->sector_count << disk->min_block_size_shift);

    return seq_printf(seq, "%s%d %d %d %lld %lld\n", disk->name, part->part_number, disk->major, disk->first_minor + part->part_number, (uint64_t)part->first_sector << disk->min_block_size_shift, (uint64_t)part->sector_count << disk->min_block_size_shift);
}

static int disk_seq_render(struct seq_file *seq)
{
    struct disk *disk = seq_list_get_entry(seq, struct disk, disk_entry);

    int i;
    for (i = 0; i < disk->part_count; i++) {
        int ret = disk_create_name(seq, disk, disk->parts[i]);
        if (ret < 0)
            return ret;
    }

    return 0;
}

static int disk_seq_next(struct seq_file *seq)
{
    return seq_list_next(seq, &disk_list);
}

const static struct seq_file_ops disk_seq_file_ops = {
    .start = disk_seq_start,
    .end = disk_seq_end,
    .render = disk_seq_render,
    .next = disk_seq_next,
};

static int disk_file_seq_open(struct inode *inode, struct file *filp)
{
    return seq_open(filp, &disk_seq_file_ops);
}

const struct file_ops disk_file_ops = {
    .open = disk_file_seq_open,
    .lseek = seq_lseek,
    .read = seq_read,
    .release = seq_release,
};
