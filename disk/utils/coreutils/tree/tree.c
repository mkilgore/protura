// tree - List directory contents in tree format
#define UTILITY_NAME "tree"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [Directory]";
static const char *usage_str = "Display contents of Directory in tree format.\n";
static const char *arg_desc_str  = "Directory: Directory to display. Defaults to current directory.\n";

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

void print_dir(int indent)
{
     if 
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    const char *dir = NULL;
    int err;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            dir = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (dir) {
        err = chdir(dir);
        if (err) {
            perror(dir);
            return 1;
        }
    }

    print_dir(0);

    return 0;
}

