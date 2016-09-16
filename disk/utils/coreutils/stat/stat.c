// stat - Get information on a file
#define UTILITY_NAME "stat"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [File]";
static const char *usage_str = "Display information on File.\n";
static const char *arg_desc_str = "File: Any node on the filesystem.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(dereference, "dereference", 'L', 0, NULL, "Follow links") \
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

static int follow_links = 0;
static const char *file = NULL;

const char *mode_to_string(mode_t mode)
{
    if (S_ISREG(mode))
        return "regular file";
    else if (S_ISDIR(mode))
        return "directory";
    else if (S_ISLNK(mode))
        return "symbolic link";
    else if (S_ISBLK(mode))
        return "block device";
    else if (S_ISCHR(mode))
        return "character device";
    else if (S_ISFIFO(mode))
        return "fifo device";

    return "";
}

void print_slink(const char *file, struct stat *st)
{
}

void print_stat(const char *file, struct stat *st)
{
    if (S_ISLNK(st->st_mode)) {
        char buf[256] = { 0 };
        int ret;

        ret = readlink(file, buf, sizeof(buf));
        if (ret)
            perror(file);

        printf("  File: \"%s\" -> \"%s\"\n", file, buf);
    } else {
        printf("  File: \"%s\"\n", file);
    }

    printf("  Size: %-10d Blocks: %-10d IO Block: %-10d %s\n",
            (int)st->st_size,
            (int)st->st_blocks,
            (int)st->st_blksize,
            mode_to_string(st->st_mode));
    if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
        printf("Device: %4d,%-5d  Inode: %-10d    Links: %-10d Refed Device: %d,%d\n",
                (int)major(st->st_dev), (int)minor(st->st_dev),
                (int)st->st_ino,
                (int)st->st_nlink,
                (int)major(st->st_rdev), (int)minor(st->st_rdev));
    } else {
        printf("Device: %4d,%-5d  Inode: %-10d    Links: %-10d\n",
                (int)major(st->st_dev), (int)minor(st->st_dev),
                (int)st->st_ino,
                (int)st->st_nlink);
    }
}

int main(int argc, char **argv) {
    enum arg_index ret;
    struct stat st;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_dereference:
            follow_links = 1;
            break;

        case ARG_EXTRA:
            if (file) {
                fprintf(stderr, "%s: Too many arguments.\n", argv[0]);
                return 0;
            }

            file = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (follow_links)
        stat(file, &st);
    else
        lstat(file, &st);

    print_stat(file, &st);

    return 0;
}

