#ifndef INCLUDE_PROTURA_BLOCK_BDEV_H
#define INCLUDE_PROTURA_BLOCK_BDEV_H

#include <protura/types.h>
#include <arch/spinlock.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/scheduler.h>
#include <protura/fs/file.h>
#include <protura/mutex.h>
#include <protura/dev.h>
#include <protura/block/statvfs.h>

struct disk;
struct disk_part;
struct block;
struct block_device;

enum {
    BDEV_INITIALIZED,
};

struct block_device {
    dev_t dev;
    flags_t flags;

    hlist_node_t hash_entry;

    list_head_t blocks;

    /* Protects open_refs, and serializes open/close events:
     *   repartitioning (BDEV_INITIALIZED)
     *   initial block-size
     *
     * These are read-only when open_refs>0:
     *   ->name
     *   ->disk
     *   ->part
     *   ->whole
     */
    mutex_t lock;
    int refs;
    int open_refs;

    /* Filled in from the disk name at open time */
    char name[28];

    spinlock_t block_size_lock;
    size_t block_size;

    /* 'whole' points back to the block_device representing the whole disk if
     * this is a partition */
    struct block_device *whole;

    /* Only valid when the block_device has been opened */
    struct disk *disk;
    struct disk_part *part;

    const struct file_ops *fops;
};

#define BLOCK_DEVICE_INIT(bdev) \
    { \
        .blocks = LIST_HEAD_INIT((bdev).blocks), \
        .lock = MUTEX_INIT((bdev).lock), \
    }

static inline void block_device_init(struct block_device *bdev)
{
    *bdev = (struct block_device)BLOCK_DEVICE_INIT(*bdev);
}

extern struct file_ops block_dev_file_ops_generic;

int block_dev_file_open_generic(struct inode *dev, struct file *filp);
int block_dev_file_close_generic(struct file *);

enum {
    BLOCK_DEV_NONE = 0,
    BLOCK_DEV_IDE_MASTER = 1,
    BLOCK_DEV_IDE_SLAVE = 2,
    BLOCK_DEV_ANON = 3,
    BLOCK_DEV_ATA = 4,
    BLOCK_DEV_LOOP = 5,
};

struct block_device *block_dev_get(dev_t device);
void block_dev_put(struct block_device *);

/* Only works if there are no open references to partition block-devices */
int block_dev_repartition(struct block_device *);

/* Opens and closes an active reference to this block_device
 * This will prevent certain actions like repartitioning from happening
 * Use FILE_EXCL for exclusive access */
int block_dev_open(struct block_device *, flags_t file_flags);
void block_dev_close(struct block_device *);

void block_dev_block_size_set(struct block_device *, size_t size);
size_t block_dev_block_size_get(struct block_device *);
uint64_t block_dev_capacity_get(struct block_device *);

void block_dev_clear(struct block_device *);
void block_dev_sync(struct block_device *, int wait);

void block_sync_all(int wait);
void block_submit(struct block *);

dev_t block_dev_anon_get(void);
void block_dev_anon_put(dev_t dev);

#endif
