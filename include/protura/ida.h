#ifndef INCLUDE_PROTURA_IDA_H
#define INCLUDE_PROTURA_IDA_H

#include <protura/types.h>
#include <protura/spinlock.h>

struct ida {
    spinlock_t lock;
    int total_ids;
    uint32_t *ids;
};

#define IDA_INIT(id_array, total) \
    { \
        .lock = SPINLOCK_INIT(), \
        .ids = (id_array), \
        .total_ids = (total), \
    }

static inline void ida_init(struct ida *ida, uint32_t *ids, int total_ids)
{
    *ida = (struct ida)IDA_INIT(ids, total_ids);
}

/* Returns -1 if the allocation can't be done */
int ida_getid(struct ida *);
void ida_putid(struct ida *, int id);

#endif
