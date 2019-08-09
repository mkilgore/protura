// more - Output pager
#define UTILITY_NAME "more"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <termios.h>

#include "arg_parser.h"
#include "file.h"

#define MAX_FILES 50

static const char *arg_str = "[Flags] [File...]";
static const char *usage_str = "More allows you to view a long output a screen or line at a time.\n";
static const char *arg_desc_str  = "File...: File or Files to display.\n"
                                   "         By default stdin will be displayed.\n";

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

static int file_count = 0;
static FILE *file_list[MAX_FILES] = { 0 };

static int tty_fd;
static int screen_rows = 25;

int page_file(FILE *file)
{
    int row = 0;
    int hit_eof = 0;
    char *line = NULL;
    size_t buf_len = 0;
    size_t len;

    while (!hit_eof) {

        for (row = 0; row < screen_rows - 1&& !hit_eof; row++) {
            len = getline(&line, &buf_len, file);
            hit_eof = len == -1;

            if (line && !hit_eof)
                printf("%s", line);
        }

        if (!hit_eof) {
            printf("-- MORE --");
            fflush(stdout);

            struct pollfd pollfd;
            memset(&pollfd, 0, sizeof(pollfd));

            pollfd.fd = tty_fd;
            pollfd.events = POLLIN;
            do {
                pollfd.revents = 0;
                poll(&pollfd, 1, -1);
            } while (!(pollfd.revents & POLLIN));

            /* Clear out the newline character */
            char c;
            read(tty_fd, &c, 1);

            printf("\r          \r");
            fflush(stdout);
        }
    }

    if (line)
        free(line);

    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    struct termios old_term, term;
    struct winsize wsize;
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
            if (file_count == MAX_FILES) {
                printf("%s: Error, max number of outputs is %d\n", argv[0], MAX_FILES);
                return 0;
            }
            file_list[file_count] = fopen_with_dash(argarg, "r");
            if (!file_list[file_count]) {
                perror(argarg);
                return 1;
            }
            file_count++;
            break;


        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!file_count) {
        file_count = 1;
        file_list[0] = fopen_with_dash("-", "r");
    }

    tty_fd = open("/dev/tty", O_RDONLY);
    if (tty_fd == -1) {
        perror(argv[0]);
        return 1;
    }

    tcgetattr(tty_fd, &old_term);
    ioctl(tty_fd, TIOCGWINSZ, &wsize);

    screen_rows = wsize.ws_row;

    term = old_term;
    term.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL);

    tcsetattr(tty_fd, TCSANOW, &term);

    for (i = 0; i < file_count; i++) {
        page_file(file_list[i]);
        fclose_with_dash(file_list[i]);
    }

    tcsetattr(tty_fd, TCSANOW, &old_term);

    return 0;
}

