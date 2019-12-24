// truncate - Create or 'truncate' a file
#define UTILITY_NAME "truncate"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "arg_parser.h"

static const char *arg_str = "[Flags] [File]";
static const char *usage_str = "Shrink or extend the size of the provided file.\n";
static const char *arg_desc_str  = "";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(io_blocks, "io-blocks", 'o', 0, NULL, "Treat size as number of IO blocks") \
    X(size, "size", 's', 1, "Size to truncate too", "Size for truncating, can be prefixed with '+' or '-'") \
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

int main(int argc, char **argv)
{
    enum arg_index ret;
    int is_blocks = 0;
    int is_offset = 0;
    uint32_t size = 0;
    const char *file = NULL;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_io_blocks:
            is_blocks = 1;
            break;

        case ARG_size:
            switch (argarg[0]) {
            case '+':
                is_offset = 1;
                break;

            case '-':
                is_offset = -1;
                break;
            }

            size = atoi(argarg);
            break;

        case ARG_EXTRA:
            if (file) {
                printf("%s: Please only supply one file name\n", argv[0]);
                return 1;
            }
            file = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    int err = 0;

    if (!file) {
        printf("%s: Please supply a file to truncate\n", argv[0]);
        return 0;
    }

    struct stat st;
    memset(&st, 0, sizeof(st));

    err = stat(file, &st);
    if (err) {
        perror(file);
        return 1;
    }

    if (is_blocks)
        size *= st.st_blksize;

    off_t truncate_len;

    if (is_offset == 1)
        truncate_len = st.st_size + size;
    else if (is_offset == -1)
        truncate_len = st.st_size - size;
    else
        truncate_len = size;

    err = truncate(file, truncate_len);
    if (err) {
        perror(file);
        return 1;
    }

    return 0;
}

