// ln - Create links between files
#define UTILITY_NAME "ln"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [Target] [Link_Name]";
static const char *usage_str = "Create links (hard or symbolic) bewteen files.\n";
static const char *arg_desc_str  = "Target: The target of the link to create.\n"
                                   "Link_Name: The link to create.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(symbolic, "symbolic", 's', 0, NULL, "Creating symbolic instead of hard links") \
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

enum link_type {
    LINK_HARD,
    LINK_SYMBOLIC,
};

static enum link_type link_type = LINK_HARD;

static const char *target, *linkname;

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

        case ARG_symbolic:
            link_type = LINK_SYMBOLIC;
            break;

        case ARG_EXTRA:
            if (!target) {
                target = argarg;
            } else if (!linkname) {
                linkname = argarg;
            } else {
                fprintf(stderr, "%s: Too many arguments.\n", argv[0]);
                return 0;
            }
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    switch (link_type) {
    case LINK_HARD:
        err = link(target, linkname);
        break;

    case LINK_SYMBOLIC:
        err = symlink(target, linkname);
        break;
    }

    if (err)
        perror(argv[0]);

    return 0;
}

