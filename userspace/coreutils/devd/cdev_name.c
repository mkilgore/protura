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

static void set_default(struct device *device)
{
    a_sprintf(&device->name, "chr:%d:%d", major(device->dev), minor(device->dev));
    device->mode |= 0600;
}

static void tty_name(struct device *device)
{
    if (minor(device->dev))
        a_sprintf(&device->name, "tty%d", minor(device->dev) - 1);
    else
        device->name = strdup("tty");

    struct group *ent = group_db_get_group(&groupdb, "tty");
    if (ent)
        device->gid = ent->gid;

    device->mode |= 0666;
}

static void tty_serial_name(struct device *device)
{
    struct group *ent = group_db_get_group(&groupdb, "tty");
    if (ent)
        device->gid = ent->gid;

    a_sprintf(&device->name, "ttyS%d", minor(device->dev));
    device->mode |= 0666;
}

static void mem_name(struct device *device)
{
    struct group *ent;

    switch (minor(device->dev)) {
    case 0:
        device->name = strdup("zero");
        device->mode |= 0666;
        break;

    case 1:
        device->name = strdup("full");
        device->mode |= 0666;
        break;

    case 2:
        device->name = strdup("null");
        device->mode |= 0666;
        break;

    case 3:
        device->name = strdup("loop-control");

        ent = group_db_get_group(&groupdb, "disk");
        if (ent)
            device->gid = ent->gid;

        device->mode |= 0660;
        break;

    default:
        set_default(device);
        break;
    }
}

static void qemudbg_name(struct device *device)
{
    if (minor(device->dev))
        return set_default(device);

    device->name = strdup("qemudbg");
    device->mode |= 0666;
}

static void fb_name(struct device *device)
{
    struct group *ent = group_db_get_group(&groupdb, "video");
    if (ent)
        device->gid = ent->gid;

    a_sprintf(&device->name, "fb%d", minor(device->dev));
    device->mode |= 0666;
}

static void event_name(struct device *device)
{
    switch (minor(device->dev)) {
    case 0:
        device->name = strdup("keyboard");
        device->mode |= 0666;
        break;

    default:
        set_default(device);
        break;
    }
}

static void (*get_name[]) (struct device *) = {
    [5] = tty_name,
    [6] = mem_name,
    [7] = tty_serial_name,
    [8] = qemudbg_name,
    [9] = fb_name,
    [10] = event_name,
};

void cdev_create(struct device *device)
{
    if (major(device->dev) < ARRAY_SIZE(get_name) && get_name[major(device->dev)])
        return (get_name[major(device->dev)]) (device);

    /* Junk */
    set_default(device);
}
