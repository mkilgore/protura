// chmod - change file permissions
#define UTILITY_NAME "chmod"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] mode [Files]";
static const char *usage_str = "Change permissions on a file or files.\n";
static const char *arg_desc_str = "Files: List of files to apply permissions too.\n";

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

static const char *prog;

static int octal_mode_to_int(const char *str, mode_t *val)
{
    char *endp = NULL;
    long lval = strtol(str, &endp, 8);

    if (*endp)
        return 1;

    if (lval > 07777 || lval < 0)
        return 1;

    *val = lval;

    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int have_mode = 0;
    mode_t new_mode;

    prog = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            if (!have_mode) {
                int err = octal_mode_to_int(argarg, &new_mode);
                if (err) {
                    fprintf(stderr, "%s: Invalid mode: %s\n", prog, argarg);
                    return 1;
                }

                have_mode = 1;
            } else {
                int err = chmod(argarg, new_mode);
                if (err) {
                    fprintf(stderr, "%s: %s: %s\n", prog, argarg, strerror(errno));
                    return 1;
                }
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    return 0;
}
