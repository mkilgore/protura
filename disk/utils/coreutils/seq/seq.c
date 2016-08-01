// seq - Print a sequence of numbers

#define UTILITY_NAME "seq"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [First] [Increment] Last";
static const char *usage_str = "Print a sequence of numbers from First to Last\n";
static const char *arg_desc_str  = "First: Number to start counting from. Defaults to 1.\n"
                                   "Increment: Number to increment 'First' by. Defaults to 1.\n"
                                   "Last: Number to count too. Must be provided.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(separator, "separator", 's', 1, "String", "Use 'String' to separate numbers. Default '\\n'.") \
    X(last, NULL, '\0', 0, NULL, NULL)

enum arg_index {
  ARG_EXTRA = ARG_PARSER_EXTRA,
  ARG_ERR = ARG_PARSER_ERR,
  ARG_DONE = ARG_PARSER_DONE,
#define X(enu, ...) ARG_ENUM(enu)
  XARGS
#undef X
};

static const struct arg seq_args[] = {
#define X(...) CREATE_ARG(__VA_ARGS__)
  XARGS
#undef X
};

static const char *sep = "\n";

static int first = 1, inc = 1, last;

int main(int argc, char **argv) {
    int extra_count = 0, i;
    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, seq_args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, seq_args);
            break;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_separator:
            sep = argarg;
            break;

        case ARG_EXTRA:
            if (extra_count == 0) {
                last = strtol(argarg, NULL, 0);
            } else if (extra_count == 1) {
                first = last;
                last = strtol(argarg, NULL, 0);
            } else if (extra_count == 2) {
                inc = last;
                last = strtol(argarg, NULL, 0);
            }
            extra_count++;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    for (i = first; i <= last; i += inc)
        printf("%d%s", i, sep);

    if (strcmp(sep, "\n") != 0)
        putchar('\n');

    return 0;
}

