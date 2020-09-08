// devd - Daemon for creating and managing /dev entries
#define UTILITY_NAME "devd"

#include "common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/sysmacros.h>
#include <protura/event/device.h>
#include <protura/event/protocol.h>

#include "internal.h"

static char disk_id_to_char(int disk_id)
{
    if (disk_id < 26)
        return 'a' + disk_id;
    else
        return '?'; /* FIXME: we should do 'aa', 'ab', etc... */
}

static void ata_name(struct device *device)
{
    int disk_id = minor(device->dev) / 256;
    int partition = minor(device->dev) % 256;

    if (partition)
        a_sprintf(&device->name, "hd%c%d", disk_id_to_char(disk_id), partition);
    else
        a_sprintf(&device->name, "hd%c", disk_id_to_char(disk_id));

    struct group *ent = group_db_get_group(&groupdb, "disk");
    if (ent)
        device->gid = ent->gid;

    device->mode |= 0660;
}

void bdev_create(struct device *device)
{
    switch (major(device->dev)) {
    case 4:
        return ata_name(device);

    default: /* Junk */
        a_sprintf(&device->name, "blk:%d:%d", major(device->dev), minor(device->dev));
        device->mode |= 0600;
        break;
    }
}
