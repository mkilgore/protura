#ifndef INCLUDE_PROTURA_FS_MBR_PART_H
#define INCLUDE_PROTURA_FS_MBR_PART_H

#include <protura/fs/block.h>

int mbr_add_partitions(struct block_device *device);

#endif
