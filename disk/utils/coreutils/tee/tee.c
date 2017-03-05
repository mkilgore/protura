// tee - read from stdin and write to file(s) and stdout
#define UTILITY_NAME "tee"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"
#include "file.h"

#define MAX_FILES 50
#define BUF_SIZE 1024

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Read from standard input and write to Files and standard output\n";
static const char *arg_desc_str  = "Files: One or more files to write the input from stdin too.\n";

#define XARGS \
    X(append, "append", 'a', 0, NULL, "Append to files instead of overwriting") \
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

void read_write_files(void);

static bool append = false;

static int file_count = 0;
static int file_list[MAX_FILES] = { 0 };

int main(int argc, char **argv) {
    enum arg_index ret;

    file_list[file_count++] = STDOUT_FILENO;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            break;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_append:
            append = true;
            break;

        case ARG_EXTRA:
            if (file_count == MAX_FILES) {
                printf("%s: Error, max number of outputs is %d\n", argv[0], MAX_FILES);
                return 0;
            }
            file_list[file_count] = open_with_dash(argarg, O_WRONLY | O_CREAT | O_APPEND, 0777);
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

    read_write_files();

    return 0;
}

void read_write_files(void) {
    char buffer[BUF_SIZE];
    int len, i;
    int readfd = STDIN_FILENO;

    do {
        len = read(readfd, buffer, sizeof(buffer));
        if (len == 0)
            break;

        if (len == -1) {
            perror("stdin");
            break;
        }

        for (i = 0; i < file_count; i++)
            write(file_list[i], buffer, len);
    } while (1);

    return ;
}

