#ifndef INCLUDE_INTERNAL_H
#define INCLUDE_INTERNAL_H

#include "list.h"
#include "db_group.h"

struct device {
    dev_t dev;
    uid_t uid;
    gid_t gid;
    mode_t mode;

    char *name;

    list_node_t entry;
};

void bdev_create(struct device *);
void cdev_create(struct device *);

extern struct group_db groupdb;

#endif
