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

#define MAX_FILES 50

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
static int file_list[MAX_FILES] = { 0 };

static off_t total_length = -1;
static off_t starting_byte = 0;

int output_hex(int fd)
{
    off_t len;
    char *buf;

    if (total_length == -1) {
        len = lseek(fd, 0, SEEK_END);
        len -= starting_byte;
    } else {
        len = total_length;
    }

    lseek(fd, starting_byte, SEEK_SET);

    buf = malloc(len);

    read(fd, buf, len);
    dump_mem(buf, len, starting_byte);

    free(buf);

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

        case ARG_skip:
            starting_byte = atoi(argarg);
            break;

        case ARG_length:
            total_length = atoi(argarg);
            break;

        case ARG_EXTRA:
            if (file_count == MAX_FILES) {
                printf("%s: Error, max number of outputs is %d\n", argv[0], MAX_FILES);
                return 0;
            }
            file_list[file_count] = open_with_dash(argarg, O_RDONLY, 0);
            if (file_list[file_count] == -1) {
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
        file_list[0] = STDIN_FILENO;
    }

    for (i = 0; i < file_count; i++) {
        output_hex(file_list[i]);
        close_with_dash(file_list[i]);
    }

    return 0;
}

