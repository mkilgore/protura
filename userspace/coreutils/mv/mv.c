// mv - move or rename files
#define UTILITY_NAME "mv"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "arg_parser.h"

#define MAX_FILES 50

static const char *arg_str = "[Options] Source... Target";
static const char *usage_str = "Make Source to Target, or move Sources into directory Target.\n";
static const char *arg_desc_str  = "Source(s): With file(s) to move (rename).\n"
                                   "Target: The new name for Source, or directory to move multiple Sources into.\n";

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
static const char *file_list[MAX_FILES] = { 0 };

static const char *target;

int main(int argc, char **argv) {
    enum arg_index ret;
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
            file_list[file_count++] = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (file_count < 2)
        return 1;

    target = file_list[--file_count];

    if (file_count == 1) {
        err = rename(file_list[0], target);
        if (err)
            perror(file_list[0]);
    } else {
        /* Move into directory */
    }

    return 0;
}

