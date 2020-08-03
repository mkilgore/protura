#ifndef INCLUDE_PROTURA_BLOCK_DISK_H
#define INCLUDE_PROTURA_BLOCK_DISK_H

#include <protura/types.h>
#include <protura/dev.h>
#include <protura/bits.h>
#include <protura/list.h>
#include <protura/hlist.h>
#include <protura/fs/file.h>

struct disk;
struct file_ops;
struct block;
struct block_device;

struct disk_part {
    sector_t first_sector;
    sector_t sector_count;
    int part_number;
};

struct disk_ops {
    void (*sync_block) (struct disk *, struct block *);

    /* Called when the last reference to this disk is dropped */
    void (*put) (struct disk *);
};

enum {
    DISK_UP,
};

struct disk {
    char name[28];

    int major;
    int first_minor;
    int minor_count;

    /* Minimum block size, as (1 << min_block_size_shift) */
    int min_block_size_shift;

    /* These are protected by the disk hashtable lock*/
    flags_t flags;
    int refs;
    int open_refs;

    /* First partition is the whole disk
     * Partitions cannot be removed while the disk is active */
    int part_count;
    struct disk_part whole;
    struct disk_part **parts;

    const struct disk_ops *ops;

    void *priv;

    hlist_node_t hash_entry;
    list_node_t disk_entry;
};

#define DISK_INIT(disk) \
    { \
        .disk_entry = LIST_NODE_INIT((disk).disk_entry), \
    }

static inline void disk_init(struct disk *disk)
{
    *disk = (struct disk)DISK_INIT(*disk);
}

/* The returned disk already has one reference taken, which should be released
 * with disk_put() */
struct disk *disk_alloc(void);

/* These should be added with `disk_part_add()` and will be free'd
 * automatically when necessary */
struct disk_part *disk_part_alloc(void);

int disk_register(struct disk *);

/* Unregister prevents the disk from further being returned from disk_get().
 * The reference still needs to be dropped! */
void disk_unregister(struct disk *);

/* Finds and acquires a reference to the disk for the given dev_t */
struct disk *disk_get(dev_t dev, int *partno);
struct disk *disk_dup(struct disk *);

/* When the last ref is dropped, the disk is free'd */
void disk_put(struct disk *);

void disk_capacity_set(struct disk *, sector_t sectors);
sector_t disk_capacity_get(struct disk *);

void disk_part_add(struct disk *, struct disk_part *);
struct disk_part *disk_part_get(struct disk *, int partno);

static inline dev_t disk_dev_get(struct disk *disk, int partno)
{
    return DEV_MAKE(disk->major, disk->first_minor + partno);
}

/* Attempts to clear out the existing set of partitions.
 * Returns -EBUSY if the disk is currently in use */
int disk_part_clear(struct disk *);

/* Acquires an "open" reference, which will keep certain operations (like
 * clearing the partition table) from happening on the disk.
 *
 * Returns error if the disk cannot be opened (Ex. It's been removed from the machine). */
int disk_open(struct disk *);
void disk_close(struct disk *);

extern const struct file_ops disk_file_ops;

#endif
