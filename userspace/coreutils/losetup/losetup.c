// losetup - Setup a loop block device
#define UTILITY_NAME "losetup"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <protura/drivers/loop.h>

#include "arg_parser.h"

static const char *arg_str = "";
static const char *usage_str = "Setup a loop block device";
static const char *arg_desc_str  = "";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(remove, "remove", 'r', 0, NULL, "Remove a loop device") \
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

const char *prog_name;

static int loop_create(int loopctl, const char *arg)
{
    int fd = open(arg, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "%s: %s: %s\n", prog_name, arg, strerror(errno));
        return 1;
    }

    struct loopctl_create create;
    memset(&create, 0, sizeof(create));

    create.fd = fd;
    strncpy(create.loop_name, arg, LOOP_NAME_MAX);

    printf("Name: %s\n", create.loop_name);

    int err = ioctl(loopctl, LOOPCTL_CREATE, &create);

    if (err == -1) {
        fprintf(stderr, "%s: ioctl: %s!\n", prog_name, strerror(errno));
        return 1;
    }

    printf("Created /dev/loop%d\n", create.loop_number);

    return 0;
}

static int loop_remove(int loopctl, const char *arg)
{
    int id = atoi(arg);
    printf("Removing /dev/loop%d\n", id);

    struct loopctl_destroy destroy;
    memset(&destroy, 0, sizeof(destroy));

    destroy.loop_number = id;
    int err = ioctl(loopctl, LOOPCTL_DESTROY, &destroy);

    if (err == -1) {
        fprintf(stderr, "%s: ioctl: %s!\n", prog_name, strerror(errno));
        return 1;
    }

    return 0;
}

static int loop_status(int loopctl)
{
    int id = 0;
    int err = 0;

    for (; id < 256; id++) {
        struct loopctl_status status;
        memset(&status, 0, sizeof(status));

        status.loop_number = id;

        err = ioctl(loopctl, LOOPCTL_STATUS, &status);
        if (err == -1) {
            if (errno != ENOENT)
                fprintf(stderr, "%s: ioctl: %s\n", prog_name, strerror(-err));
        } else {
            printf("/dev/loop%d: %s\n", id, status.loop_name);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int fd = -1;
    int should_remove = 0;
    const char *extra_arg = NULL;

    prog_name = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_remove:
            should_remove = 1;
            break;

        case ARG_EXTRA:
            if (!extra_arg) {
                extra_arg = argarg;
            } else {
                fprintf(stderr, "%s: Unexpected argument '%s'\n", prog_name, argarg);
                return 1;
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    int loopctl = open("/dev/loop-control", O_RDWR);
    if (loopctl == -1) {
        fprintf(stderr, "%s: Unable to open /dev/loop-control: %s\n", argv[0], strerror(errno));
        return 1;
    }

    if (extra_arg && !should_remove)
        return loop_create(loopctl, extra_arg);
    else if (extra_arg && should_remove)
        return loop_remove(loopctl, extra_arg);

    return loop_status(loopctl);
}

