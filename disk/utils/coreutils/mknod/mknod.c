// mknod - Make a new node in the file system
#define UTILITY_NAME "mknod"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sysmacros.h>

#include "arg_parser.h"

static const char *arg_str = "Name Type [Major Minor]";
static const char *usage_str = "Make node Name. If type is char or block, [Major Minor] is the device number\n";
static const char *arg_desc_str = "Name: Name of node to create\n"
                                  "Type: 'c' or 'b'\n"
                                  "Major Minor: Major and Minor device number\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
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

static const char *prog;
static char type;
static const char *node;
static int dev_major, dev_minor;

int main(int argc, char **argv) {
    enum arg_index ret;
    mode_t extra;

    prog = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            if (!node)
                node = argarg;
            else if (!type)
                type = *argarg;
            else if (!dev_major)
                dev_major = atoi(argarg);
            else if (!dev_minor)
                dev_minor = atoi(argarg);

            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    switch (type) {
    case 'c':
        extra = S_IFCHR;
        break;

    case 'b':
        extra = S_IFBLK;
        break;

    default:
        printf("%s: Unreconized type: %c\n", prog, type);
        return 0;
    }

    printf("dev_major: %d, dev_minor: %d\n", dev_major, dev_minor);
    printf("direct dev: %d\n", ((dev_major) << 16) | ((dev_minor) & 0xFFFF));
    printf("dev: %d\n", makedev(dev_major, dev_minor));

    ret = mknod(node, 0777 | extra, makedev(dev_major, dev_minor));

    if (ret)
        perror(prog);

    return 0;
}

