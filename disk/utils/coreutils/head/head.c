// head - Output the first characters in a file
#define UTILITY_NAME "head"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"
#include "file.h"

#define BUFSZ 1024

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Print the first 10 lines of each File to standard output.\n";
static const char *arg_desc_str  = "Files: List of one or more files to read and output to standard out.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(bytes, "bytes", 'c', 1, "num", "Print the first [num] bytes") \
    X(lines, "lines", 'l', 1, "num", "Print the first [num] lines") \
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

enum output_type
{
    OUTPUT_LINES,
    OUTPUT_BYTES,
};

static const char *prog;

static enum output_type output_type = OUTPUT_LINES;
static int output_count = 10;

void output_beginning(int infile, int outfile);

int main(int argc, char **argv) {
    enum arg_index ret;
    int file;

    prog = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_bytes:
            output_type = OUTPUT_BYTES;
            output_count = strtol(argarg, NULL, 10);
            break;

        case ARG_lines:
            output_type = OUTPUT_LINES;
            output_count = strtol(argarg, NULL, 10);
            break;

        case ARG_EXTRA:
            file = open_with_dash(argarg, O_RDONLY);
            if (file == -1) {
                perror(argarg);
                return 1;
            }
            output_beginning(file, STDOUT_FILENO);
            close_with_dash(file);
            return 0;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    output_beginning(STDIN_FILENO, STDOUT_FILENO);

    return 0;
}

void output_beginning(int infile, int outfile)
{
    char buf[BUFSZ];
    int count = 0;
    int ret;

    do {
        int i;

        ret = read(infile, buf, sizeof(buf));
        if (ret == 0)
            break;

        if (ret == -1) {
            perror(prog);
            break;
        }

        for (i = 0; i < ret; i++) {
            switch (output_type) {
            case OUTPUT_BYTES:
                count++;

                if (count == output_count)
                    goto write_buf_to_i;

                break;

            case OUTPUT_LINES:
                if (buf[i] == '\n')
                    count++;

                if (count == output_count)
                    goto write_buf_to_i;

                break;
            }
        }

    write_buf_to_i:

        write(outfile, buf, i + 1);

        if (count == output_count)
            break;
    } while (1);
}

