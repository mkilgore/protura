// umount - unmount a filesystem
#define UTILITY_NAME "umount"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [Mountpoint]";
static const char *usage_str = "Unmount a filesystem mounted on [Mountpoint].\n";
static const char *arg_desc_str  = "Mountpoint: Location to umount.\n";

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

int main(int argc, char **argv)
{
    enum arg_index ret;
    int err;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            err = umount(argarg);
            if (err)
                perror(argv[0]);
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    return 0;
}

