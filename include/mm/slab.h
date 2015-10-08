#ifndef INCLUDE_MM_SLAB_H
#define INCLUDE_MM_SLAB_H

#include <protura/types.h>
#include <arch/spinlock.h>

struct slab_page_frame;

struct slab_alloc {
    const char *slab_name;
    int object_size;

    spinlock_t lock;

    struct slab_page_frame *first_frame;
};

#define SLAB_ALLOC_INIT(name, size) \
    { \
        .slab_name = (name), \
        .object_size = (size), \
        .lock = SPINLOCK_INIT("slab_alloc"), \
        .first_frame = NULL, \
    }

void slab_info(struct slab_alloc *, char *buf, ksize_t buf_size);

void *slab_malloc(struct slab_alloc *, unsigned int flags);
void slab_free(struct slab_alloc *, void *);

int slab_has_addr(struct slab_alloc *slab, void *addr);
void slab_clear(struct slab_alloc *);

#endif
