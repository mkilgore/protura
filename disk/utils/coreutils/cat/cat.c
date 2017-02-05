// cat - read and output files
#define UTILITY_NAME "cat"

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "arg_parser.h"
#include "file.h"

static const char *args_str = "[Flags] [Files]";
static const char *usage_str  = "Concatentate Files and/or standard input to standard output.\n";
static const char *arg_desc_str = "Files: List of files to Concatenate into output.\n"
                                  "       Defaults to standard in, and the file '-' refers to\n"
                                  "       standard in\n";

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

void output_file(int in, const char *filename, int out);

int main(int argc, char **argv) {
    bool has_file = false;
    int file;
    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], args_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_EXTRA:
            has_file = true;
            file = open_with_dash(argarg, O_RDONLY);
            if (file == -1) {
                perror(argarg);
                return 1;
            }
            output_file(file, argarg, STDOUT_FILENO);
            close_with_dash(file);
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!has_file)
        output_file(STDIN_FILENO, "stdin", STDOUT_FILENO);

    return 0;
}

#define BUFFER_SZ 1024

void output_file(int in, const char *filename, int out) {
  static char buffer[BUFFER_SZ];
  size_t sz;

  do {
      sz = read(in, buffer, sizeof(buffer));
      if (sz == 0)
          break;

      if (sz == -1) {
          perror(filename);
          return ;
      }

      write(out, buffer, sz);
  } while (1);
}

