// devd - Daemon for creating and managing /dev entries
#define UTILITY_NAME "devd"

#include "common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/poll.h>
#include <sys/sysmacros.h>
#include <protura/event/device.h>
#include <protura/event/protocol.h>

#include "list.h"
#include "arg_parser.h"
#include "db_group.h"

#include "internal.h"

#define PROC_DEVICES "/proc/devices"
#define LOGFILE "/var/log/devd.log"

const char *prog_name;
struct group_db groupdb = GROUP_DB_INIT(groupdb);

static list_head_t device_list = LIST_HEAD_INIT(device_list);

static struct device *device_add(int is_block, dev_t dev)
{
    struct device *device = malloc(sizeof(*device));
    memset(device, 0, sizeof(*device));
    list_node_init(&device->entry);

    device->mode = (is_block? S_IFBLK: S_IFCHR);
    device->dev = dev;

    if (is_block)
        bdev_create(device);
    else
        cdev_create(device);

    printf("New Device: %s (%c, %d, %d)\n", device->name, is_block? 'b': 'c', major(device->dev), minor(device->dev));

    list_add_tail(&device_list, &device->entry);

    return device;
}

static struct device *device_find(int is_block, dev_t dev)
{
    struct device *device;

    list_foreach_entry(&device_list, device, entry)
        if (S_ISBLK(device->mode) && device->dev == dev)
            return device;

    return NULL;
}

static void device_remove(struct device *device)
{
    printf("Remove Device: %s\n", device->name);
    list_del(&device->entry);

    free(device->name);
    free(device);
}

static void device_create_node(struct device *device)
{
    char path[128];
    snprintf(path, sizeof(path), "/dev/%s", device->name);
    struct stat statbuf;
    memset(&statbuf, 0, sizeof(statbuf));

    int err = stat(path, &statbuf);
    if (err || statbuf.st_rdev != device->dev || (device->mode & S_IFMT) != (statbuf.st_mode & S_IFMT)) {
        printf("(re)creating node %s!\n", path);
        unlink(path);
        mknod(path, device->mode, device->dev);

        /* Ensure we do the chown, because the mknod() redid the ownership information */
        err = -1;
    }

    if (err || statbuf.st_uid != device->uid || statbuf.st_gid != device->gid) {
        printf("chown node %s: %d, %d\n", path, device->uid, device->gid);
        chown(path, device->uid, device->gid);
    }
}

static void device_remove_node(struct device *device)
{
    char path[128];
    snprintf(path, sizeof(path), "/dev/%s", device->name);

    printf("Removing node %s!\n", path);
    unlink(path);
}

/* This removes any stale nodes in /dev, and also creates any missing nodes */
static void cleanup_dev(void)
{
    DIR *dir = opendir("/dev");
    struct dirent *ent;
    struct device *device;

    while ((ent = readdir(dir))) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0)
            continue;

        char path[128];
        snprintf(path, sizeof(path), "/dev/%s", ent->d_name);

        struct stat statbuf;
        int err = stat(path, &statbuf);

        if (err) {
            /* ??? - readdir() says it's there, but stat() says it's not */
            printf("Removing unknown entry %s!\n", ent->d_name);
            unlink(path);
            continue;
        }

        struct device *found = NULL;
        list_foreach_entry(&device_list, device, entry) {
            if (strcmp(ent->d_name, device->name) == 0) {
                printf("Found existing device node: %s!\n", ent->d_name);
                found = device;
                break;
            }
        }

        if (!found) {
            printf("Removing unknown node %s!\n", ent->d_name);
            unlink(path);
            continue;
        }

        if (device->mode != statbuf.st_mode) {
            if ((device->mode & S_IFMT) != (statbuf.st_mode & S_IFMT) || device->dev != statbuf.st_rdev) {
                printf("Incorrect mode type, removing node %s!\n", ent->d_name);
                unlink(path);
                continue;
            }

            printf("Incorrect node permissions for %s, changing %04o to %04o!\n", ent->d_name, statbuf.st_mode & 0777, device->mode & 0777);
            chmod(path, device->mode & 0777);
        }

        if (device->uid != statbuf.st_uid || device->gid != statbuf.st_gid) {
            printf("Incorrect ownership for %s, changing (%d, %d) to (%d, %d)!\n", ent->d_name, statbuf.st_uid, statbuf.st_gid, device->uid, device->gid);
            chown(path, device->uid, device->gid);
        }
    }

    list_foreach_entry(&device_list, device, entry)
        device_create_node(device);
}

/* Turns the running process into a daemon. This modifiedd version does not
 * replace stdout/stderr, since those are already open to a log file */
static int make_daemon(void)
{
    switch (fork()) {
    case 0:
        break;

    case -1:
        perror("fork()");
        return 1;

    default:
        exit(0);
    }

    /* get a new session id to disconnect us from the current tty */
    setsid();

    /* Change our current directory */
    chdir("/");

    /* Fork again so we're no longer a session leader */
    switch (fork()) {
    case 0:
        break;

    case -1:
        perror("fork()");
        return 1;

    default:
        exit(0);
    }

    int devnull = open("/dev/null", O_RDWR);
    dup2(devnull, STDIN_FILENO);

    /* if the devnull fd wasn't one of stdin/stdout/stderr, close it */
    if (devnull > 2)
        close(devnull);

    return 0;
}

/* 
 * Reads kernel events from eventfd and applies them to the `struct device` list.
 * This function exits on error or EOF. The error or 0 is returned.
 *
 * If modify_dev == 1, then entries in `/dev` will be modified when new events come in.
 * If modify_dev == 0, `/dev` entries will not be touched.
 */
int handle_device_events(int eventfd, int modify_dev)
{
    int ret;
    struct kern_event event_buffer[20];

    while ((ret = read(eventfd, event_buffer, sizeof(event_buffer)))) {
        if (ret == -1)
            return -errno;

        int event_count = ret / sizeof(struct kern_event);
        int i;

        for (i = 0; i < event_count; i++) {
            struct device *device;

            if (event_buffer[i].type != KERN_EVENT_DEVICE)
                continue;

            int is_block = event_buffer[i].code & KERN_EVENT_DEVICE_IS_BLOCK;

            switch (event_buffer[i].code & 0x7FFF) {
            case KERN_EVENT_DEVICE_ADD:
                device = device_add(is_block, event_buffer[i].value);
                if (modify_dev)
                    device_create_node(device);

                break;

            case KERN_EVENT_DEVICE_REMOVE:
                device = device_find(is_block, event_buffer[i].value);
                if (device) {
                    if (modify_dev)
                        device_remove_node(device);

                    device_remove(device);
                } else {
                    printf("Unknown device removed! (%c, %d, %d)!\n", is_block? 'b': 'c', major(event_buffer[i].value), minor(event_buffer[i].value));
                }
                break;
            }
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    prog_name = argv[0];
    int ret;

    int logfd = open(LOGFILE, O_WRONLY | O_APPEND | O_CREAT, 0644);
    dup2(logfd, STDOUT_FILENO);
    dup2(logfd, STDERR_FILENO);
    close(logfd);

    setbuf(stdin, NULL);
    setbuf(stdout, NULL);

    ret = group_db_load(&groupdb);
    if (ret) {
        printf("Unable to parse /etc/group!\n");
        return 1;
    }

    int eventfd = open(PROC_DEVICES, O_RDONLY | O_NONBLOCK);
    if (eventfd == -1) {
        printf("Unable to open /proc/devices: %s\n", strerror(errno));
        return 1;
    }

    /* First we populate the `struct device` array. Because the fd is
     * O_NONBLOCK, once all the existing events are read the function will
     * return with -EAGAIN */
    int err = handle_device_events(eventfd, 0);
    if (err != -EAGAIN) {
        printf("Unexpected error when reading events for the first time: %s!\n", strerror(-err));
        return 1;
    }

    /* Now we mess with the `/dev` entries to make them match what the kernel
     * has told us should be there */
    cleanup_dev();

    /* Undo the O_NONBLOCK so we'll wait for new events to arrive */
    fcntl(eventfd, F_SETFL, fcntl(eventfd, F_GETFL) & ~O_NONBLOCK);

    /* Now we daemonize - it's important that we cleanup `/dev` *first* so that
     * `init` is blocked until the `/dev` nodes are availiable. */
    ret = make_daemon();
    if (ret)
        return 1;

    /* This should never return */
    err = handle_device_events(eventfd, 1);
    printf("Unexpected error when reading events: %s!\n", strerror(-err));

    return 0;
}
