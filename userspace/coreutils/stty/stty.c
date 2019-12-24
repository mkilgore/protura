// stty - Print configuration information for the a tty
#define UTILITY_NAME "stty"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "tables.h"
#include "display.h"
#include "arg_parser.h"

static const char *arg_str = "[Flags]";
static const char *usage_str = "Print configuration information for the a tty.\n";
static const char *arg_desc_str  = "";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(all, "all", 'a', 0, NULL, "Display all settings") \
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

static struct termios term;
static struct winsize wsize;
static int tty_fd = STDIN_FILENO;
static enum disp_type disp_setting = DISP_DIFF;

int main(int argc, char **argv)
{
    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            printf("%s: Unexpected argument '%s'\n", argv[0], argarg);
            return 1;

        case ARG_all:
            disp_setting = DISP_ALL;
            break;

        case ARG_ERR:
        default:
            return 1;
        }
    }

    memset(&term, 0, sizeof(term));
    tcgetattr(tty_fd, &term);
    ioctl(tty_fd, TIOCGWINSZ, &wsize);

    print_winsize(&wsize);
    display_termios(&term, disp_setting);

    return 0;
}

