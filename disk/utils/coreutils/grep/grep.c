// grep - print lines that match pattern
#define UTILITY_NAME "grep"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <termios.h>
#include <signal.h>
#include <regex.h>

#include "arg_parser.h"
#include "file.h"

#define MAX_FILES 50

static const char *arg_str = "[Flags] Pattern [Files]";
static const char *usage_str = "grep searches for patterns in the input.\n";
static const char *arg_desc_str  = "Pattern: Regex pattern to match with.\n"
                                   "Files: File or Files to display.\n"
                                   "       By default stdin will be displayed.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(extend, "extended-regexp", 'E', 0, NULL, "Use Extended Regex") \
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

const char *prog_name;

static int file_count = 0;
static const char *file_names[MAX_FILES] = { NULL };
static FILE *file_list[MAX_FILES] = { 0 };

static void search_file(const char *name, FILE *file, regex_t *reg)
{
    ssize_t len = 0;
    char *line = NULL;
    size_t buf_len = 0;

    while ((len = getline(&line, &buf_len, file)) != -1) {
        line[len - 1] = '\0';
        len--;
        if (regexec(reg, line, 0, NULL, 0) == 0)
            printf("%s\n", line);
    }

    int err;
    if ((err = ferror(file)))
        fprintf(stderr, "%s: %s\n", name, strerror(err));
}

int main(int argc, char **argv)
{
    enum arg_index ret;
    const char *pattern = NULL;
    int i;
    int use_extended = 0;

    prog_name = argv[0];

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_extend:
            use_extended = 1;
            break;

        case ARG_EXTRA:
            if (!pattern) {
                pattern = argarg;
                printf("Pattern: %s\n", pattern);
                break;
            }

            if (file_count == MAX_FILES) {
                printf("%s: Error, max number of outputs is %d\n", argv[0], MAX_FILES);
                return 0;
            }
            printf("Input: %s\n", argarg);
            file_names[file_count] = argarg;
            file_list[file_count] = fopen_with_dash(argarg, "r");
            if (!file_list[file_count]) {
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
        file_names[file_count] = "-";
        file_list[0] = fopen_with_dash("-", "r");
    }

    regex_t pattern_regex;

    int reg_flags = REG_NOSUB;
    if (use_extended)
        reg_flags |= REG_EXTENDED;

    int err = regcomp(&pattern_regex, pattern, reg_flags);
    if (err) {
        char reg_error[256];
        regerror(err, &pattern_regex, reg_error, sizeof(reg_error));
        printf("%s: %s\n", prog_name, reg_error);
        return 1;
    }

    for (i = 0; i < file_count; i++) {
        search_file(file_names[i], file_list[i], &pattern_regex);
        fclose_with_dash(file_list[i]);
    }

    regfree(&pattern_regex);

    return 0;
}

