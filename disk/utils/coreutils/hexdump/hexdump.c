// hexdupm - Display the contents of a file in hexadecimal format
#define UTILITY_NAME "hexdump"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "arg_parser.h"
#include "file.h"
#include "dump_mem.h"

static const char *arg_str = "[Flags] [File...]";
static const char *usage_str = "hexdump outputs the contents of a file in hexadecimal format.\n";
static const char *arg_desc_str  = "File: File or files to output. By default stdin is used.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(length, "length", 'n', 1, "length", "Number of bytes to output (default is whole file)") \
    X(skip, "skip", 's', 1, "offset", "Number of bytes from beginning of file to skip") \
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

static off_t total_length = -1;
static off_t starting_byte = 0;

int output_hex(int fd, const char *name)
{
    int ret;
    off_t len;
    char *buf;

    if (total_length == -1) {
        ret = len = lseek(fd, 0, SEEK_END);
        if (ret == -1) {
            perror(name);
            return 1;
        }
        len -= starting_byte;
    } else {
        len = total_length;
    }

    ret = lseek(fd, starting_byte, SEEK_SET);
    if (ret == -1) {
        perror(name);
        return 1;
    }

    if (len == 0) {
        printf("%s: Empty file\n", name);
        return 1;
    }

    buf = malloc(len);

    ret = len = read(fd, buf, len);
    if (ret == -1) {
        perror(name);
        return 1;
    }

    if (!ret) {
        printf("%s: End of file\n", name);
        return 1;
    }

    dump_mem(buf, len, starting_byte);

    free(buf);

    return 0;
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    int fd;
    int err;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_skip:
            starting_byte = atoi(argarg);
            break;

        case ARG_length:
            total_length = atoi(argarg);
            break;

        case ARG_EXTRA:
            fd = open_with_dash(argarg, O_RDONLY, 0);
            if (fd == -1) {
                perror(argarg);
                return 1;
            }

            err = output_hex(fd, argarg);

            close_with_dash(fd);
            file_count++;

            if (err)
                return 1;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!file_count)
        output_hex(STDIN_FILENO, "stdin");

    return 0;
}

