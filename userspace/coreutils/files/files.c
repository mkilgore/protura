// files - Display file information about a process
#define UTILITY_NAME "files"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <protura/task_api.h>

#include "arg_parser.h"

#define TASK_API_FILE "/proc/task_api"

static const char *arg_str = "[Flags] [Pid]";
static const char *usage_str = "Display file information about process Pid.\n";
static const char *arg_desc_str  = "Pid: The Process to print file information about.\n";

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

pid_t pid;

static const char *type_ids[] = {
    [S_IFREG] = "r",
    [S_IFDIR] = "d",
    [S_IFBLK] = "b",
    [S_IFCHR] = "c",
    [S_IFLNK] = "l",
    [S_IFIFO] = "f",
};

struct task_api_file_info files;

int main(int argc, char **argv)
{
    enum arg_index ret;
    int err;
    int fd;
    int i;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            pid = atoi(argarg);
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    fd = open(TASK_API_FILE, O_RDONLY);
    if (!fd) {
        perror(TASK_API_FILE);
        return 1;
    }

    memset(&files, 0, sizeof(files));

    files.pid = pid;
    err = ioctl(fd, TASKIO_FILE_INFO, &files);
    if (err) {
        perror(TASK_API_FILE);
        return 1;
    }

    printf("FD TYPE FLAGS  INODE  MAJOR  MINOR   OFFSET     SIZE\n");

    for (i = 0; i < NOFILE; i++) {
        const char *t;
        char flags_buf[9];

        if (!files.files[i].in_use)
            continue;

        if (files.files[i].is_pipe)
            t = "p";
        else
            t = type_ids[files.files[i].mode & S_IFMT];

        snprintf(flags_buf, sizeof(flags_buf), "%c%c%c%c%c",
                (files.files[i].is_readable)? 'r': '-',
                (files.files[i].is_writable)? 'w': '-',
                (files.files[i].is_nonblock)? 'n': '-',
                (files.files[i].is_append)? 'a': '-',
                (files.files[i].is_cloexec)? 'c': '-');

        printf("%2d    %s %s %6d %6d %6d %8ld %8ld\n",
                i, t, flags_buf,
                files.files[i].inode, major(files.files[i].dev), minor(files.files[i].dev),
                files.files[i].offset, files.files[i].size);
    }

    close(fd);

    return 0;
}

