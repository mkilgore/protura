// mount - mount a filesystem
#define UTILITY_NAME "mount"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include "columns.h"
#include "arg_parser.h"

static const char *arg_str = "[Flags] [device] [dir]";
static const char *usage_str = "Mount the filesystem on [device] on [dir]\n";
static const char *arg_desc_str  = "Device: The block device containing the filesystem to mount.\n"
                                   "Dir: Directory to mount over.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(fstype, NULL, 't', 1, "fstype", "Type of filesystem to mount") \
    X(last, NULL, '\0', 0, NULL, NULL)

enum arg_index {
  ARG_EXTRA = ARG_PARSER_EXTRA,
  ARG_ERR = ARG_PARSER_ERR,
  ARG_DONE = ARG_PARSER_DONE,
#define X(enu, ...) ARG_ENUM(enu)
  XARGS
#undef X
};

static const struct arg args[] = {
#define X(...) CREATE_ARG(__VA_ARGS__)
  XARGS
#undef X
};

const char *fstype = NULL;
const char *source = NULL, *target = NULL;

const char *prog_name;

static int add_device_name(struct util_line *line, const char *device)
{
    if (*device != '(') {
        util_line_strdup(line, device);
        return 0;
    }

    int major = 0, minor = 0;

    int err = sscanf(device, "(%d,%d)", &major, &minor);
    if (err != 2) {
        printf("%s: /proc/mounts: device '%s' invalid!\n", prog_name, device);
        return 1;
    }

    dev_t dev = makedev(major, minor);

    DIR *directory = opendir("/dev");
    if (!directory)
        return -1;

    struct dirent *item;
    while ((item = readdir(directory))) {
        char path[256];

        snprintf(path, sizeof(path), "/dev/%s", item->d_name);

        struct stat st;

        int err = stat(path, &st);
        if (err == -1)
            continue;

        if (dev == st.st_rdev && S_ISBLK(st.st_mode)) {
            util_line_strdup(line, path);
            closedir(directory);
            return 0;
        }
    }

    closedir(directory);
    return -1;
}

static int display_mounts(void)
{
    int fd = open("/proc/mounts", O_RDONLY);
    if (!fd) {
        printf("%s: /proc/mounts: %s\n", prog_name, strerror(errno));
        return 1;
    }

    FILE *filp = fdopen(fd, "r");
    if (!filp) {
        printf("%s: /proc/mounts: %s\n", prog_name, strerror(errno));
        return 1;
    }

    char *line = NULL;
    size_t line_len = 0;
    ssize_t ret;

    struct util_display display = UTIL_DISPLAY_INIT(display);

    struct util_line *header = util_display_next_line(&display);
    util_line_strdup(header, "Device");
    util_line_strdup(header, "Type");
    util_line_strdup(header, "Mount Point");

    while ((ret = getline(&line, &line_len, filp)) != -1) {
        line[ret - 1] = '\0';

        char *dev, *type, *mount_point;

        char **splits[] = { &dev, &type, &mount_point, NULL };
        char *sav_ptr = NULL;
        int i = 0;

        for (char *c = strtok_r(line, "\t", &sav_ptr); c && splits[i]; i++, c = strtok_r(NULL, "\t", &sav_ptr))
            *splits[i] = c;

        if (splits[i]) {
            printf("%s: Unrecognized /proc/mounts line: '%s'\n", prog_name, line);
            continue;
        }

        struct util_line *uline = util_display_next_line(&display);

        int err = add_device_name(uline, dev);
        if (err)
            continue;

        util_line_strdup(uline, type);
        util_line_strdup(uline, mount_point);
    }

    free(line);

    util_display_render(&display);
    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int err;

    prog_name = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_fstype:
            fstype = argarg;
            break;

        case ARG_EXTRA:
            if (!source) {
                source = argarg;
            } else if (!target) {
                target = argarg;
            } else {
                fprintf(stderr, "%s: Too many arguments.\n", argv[0]);
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!source && !target)
        return display_mounts();

    if (!source || !target) {
        printf("%s: Please supply a device and mount location.", argv[0]);
        return 1;
    }

    err = mount(source, target, (fstype)? fstype: "ext2", 0, NULL);
    if (err)
        perror(argv[0]);

    return 0;
}

