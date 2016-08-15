// more - Output pager
#define UTILITY_NAME "more"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

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

static int screen_rows = 25;

int page_file(FILE *file)
{
    int row = 0;
    int hit_eof = 0;
    char *line = NULL;
    size_t buf_len = 0;
    size_t len;

    while (!hit_eof) {

        for (row = 0; row < screen_rows && !hit_eof; row++) {
            len = getline(&line, &buf_len, file);
            hit_eof = len == -1;

            if (line && !hit_eof) {
                /* Get rid of newline if we're on the last row. */
                if (row + 1 == screen_rows)
                    line[len - 1] = '\0';

                printf("%s", line);
            }
        }

        fflush(stdout);

        /* We can't rely on stdin, since we allow piped in input. The hope is
         * that there is that the underlying device (tty or otherwise) allows
         * both input and output, and stdin/stdout/stderr are all connected to
         * it. So reading from stderr gives us input from the user, which we
         * otherwise can't get. */
        if (!hit_eof) {
            fgetc(stderr);
            putchar('\n');
        }
    }

    if (line)
        free(line);

    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
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

    for (i = 0; i < file_count; i++) {
        page_file(file_list[i]);
        fclose_with_dash(file_list[i]);
    }

    return 0;
}

