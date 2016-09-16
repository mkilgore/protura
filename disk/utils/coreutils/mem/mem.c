
// mem - Display memory information about a process
#define UTILITY_NAME "mem"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <protura/task_api.h>

#include "arg_parser.h"

#define TASK_API_FILE "/proc/task_api"

static const char *arg_str = "[Flags] [Pid]";
static const char *usage_str = "Display memory information about a process.\n";
static const char *arg_desc_str  = "Pid: The Process to print information about.\n";

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

int main(int argc, char **argv)
{
    enum arg_index ret;
    int err;
    struct task_api_mem_info mem;
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
    if (!fd)
        return 1;

    mem.pid = pid;
    err = ioctl(fd, TASKIO_MEM_INFO, &mem);
    if (err) {
        perror(TASK_API_FILE);
        return 1;
    }

    printf("START      END        FLAGS\n");

    for (i = 0; i < mem.region_count; i++) {
        printf("0x%08x-0x%08x %c%c%c\n",
                (int)mem.regions[i].start,
                (int)mem.regions[i].end,
                (mem.regions[i].is_read)? 'R': '-',
                (mem.regions[i].is_write)? 'W': '-',
                (mem.regions[i].is_exec)? 'E': '-');
    }

    close(fd);

    return 0;
}

