// getty - Open a new session and tty
#define UTILITY_NAME "getty"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags]";
static const char *usage_str = "getty starts a new terminal session, and optionally opens the supplied TTY device.\n";
static const char *arg_desc_str  = "TTY device: The TTY device file to open.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(tty_device, "tty-device", 't', 1, "device", "The TTY device to open") \
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

const char *login = "/bin/sh";
const char *tty_dev = NULL;

int main(int argc, char **argv)
{
    enum arg_index ret;
    pid_t p;
    int fd;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_tty_device:
            tty_dev = argarg;
            break;

        case ARG_EXTRA:
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (tty_dev) {
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
    }

    p = setsid();

    if (tty_dev) {
        fd = open(tty_dev, O_RDWR);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }

    execl(login, login, NULL);

    return 0;
}

