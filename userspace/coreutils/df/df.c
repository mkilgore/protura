// df - Show information about the mounted file systems
#define UTILITY_NAME "df"

#include "common.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>

#include "list.h"
#include "columns.h"
#include "arg_parser.h"
#include "db_passwd.h"
#include "db_group.h"

#define PROC_MOUNT "/proc/mounts"

static const char *arg_str = "[Flags]";
static const char *usage_str = "Show information about the mounted file systems\n";
static const char *arg_desc_str  = "";

#define XARGS \
    X(help, "help", 0, '\0', "Display help") \
    X(version, "version", 0, 'v', "Display version information") \
    X(human, "human-readable", 0, 'h', "Use human readable sizes") \
    X(last, NULL, 0, '\0', NULL)

enum arg_index {
    ARG_EXTRA = ARG_PARSER_EXTRA,
    ARG_ERR = ARG_PARSER_ERR,
    ARG_DONE = ARG_PARSER_DONE,
#define X(enu, id, arg, op, help_text) ARG_##enu,
    XARGS
#undef X
};

static const struct arg df_args[] = {
#define X(enu, id, arg, op, help_text) [ARG_##enu] = { .lng = id, .shrt = op, .help_txt = help_text, .has_arg = arg },
    XARGS
#undef X
};

const char *prog_name;
static struct util_display display = UTIL_DISPLAY_INIT(display);
static int human_readable = 0;

/* Converts blocks of a given block_size into 1K blocks - rounding if necessary
 *
 * Note: Block_size is assumed to be a power-of-two */
static uint64_t convert_to_1k(uint64_t value, int block_size)
{
    if (block_size < 1024) {
        int div = 1024 / block_size;
        return value / div;
    }

    int mult = block_size / 1024;
    return value * mult;
}

static void add_fs_name(struct util_line *line, int fsid)
{
    switch (fsid) {
    case FSID_EXT2:
        util_line_strdup(line, "ext2");
        break;

    default:
        util_line_strdup(line, "unknown");
        break;
    }
}

static void display_size(struct util_line *line, uint64_t value)
{
    if (!human_readable || value < 1024)
         util_line_printf_ar(line, "%llu", value);
    else if (value < 1024 * 1024)
       util_line_printf_ar(line, "%lluM", value / 1024);
    else
        util_line_printf_ar(line, "%lluG", value / 1024 / 1024);
}

static int add_device_name(struct util_line *line, const char *fs)
{
    struct stat st;

    int err = stat(fs, &st);
    if (err == -1)
        return -1;

    if (st.st_dev == 0)
        return -1;

    dev_t dev = st.st_dev;

    DIR *directory = opendir("/dev");
    if (!directory)
        return -1;

    struct dirent *item;
    while ((item = readdir(directory))) {
        char path[256];

        snprintf(path, sizeof(path), "/dev/%s", item->d_name);

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

void print_mounts(void)
{
    FILE *f = fopen(PROC_MOUNT, "r");
    char *line = NULL;
    size_t buf_len = 0;
    ssize_t len;

    struct util_line *header = util_display_next_line(&display);
    util_line_strdup(header, "Filesystem");
    util_line_strdup_ar(header, "1K-blocks");
    util_line_strdup_ar(header, "Used");
    util_line_strdup_ar(header, "Available");
    util_line_strdup_ar(header, "Use%");
    util_line_strdup(header, "Mounted on");

    while ((len = getline(&line, &buf_len, f)) != EOF) {
        line[len - 1] = '\0';

        /* Goto second tab */
        char *mount = strrchr(line, '\t');
        struct statvfs stvfs;

        if (mount)
            mount++;
        else
            continue;

        int err = statvfs(mount, &stvfs);
        if (err) {
            if (errno == ENOTSUP)
                continue;

            printf("%s: %s: %s\n", prog_name, mount, strerror(errno));
            continue;
        }

        struct util_line *uline = util_display_next_line(&display);

        uint64_t blocks = convert_to_1k(stvfs.f_blocks, stvfs.f_bsize);
        uint64_t bfree = convert_to_1k(stvfs.f_bfree, stvfs.f_bsize);

        err = add_device_name(uline, mount);
        if (err == -1)
            add_fs_name(uline, stvfs.f_fsid);

        display_size(uline, blocks);
        display_size(uline, blocks - bfree);
        display_size(uline, bfree);
        util_line_printf_ar(uline, "%llu%%", ((blocks - bfree) * 100) / blocks);
        util_line_printf(uline, "%s", mount);
    }

    util_display_render(&display);
}

int main(int argc, char **argv)
{
    prog_name = argv[0];

    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, df_args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, df_args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_human:
            human_readable = 1;
            break;

        default:
            return 0;
        }
    }

    print_mounts();

    return 0;
}

