// uniq - remove or show repeated lines in file(s)
#define UTILITY_NAME "uniq"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg_parser.h"
#include "stringcasecmp.h"
#include "file.h"

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Remove or show repeated lines in Files\n";
static const char *arg_desc_str  = "Files: One or more files to run uniq over.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(count, "count", 'c', 0, NULL, "Prefix lines with the number of occurrences") \
    X(repeated, "repeated", 'D', 0, NULL, "Only print duplicate lines") \
    X(ignore_case, "ignore-case", 'i', 0, NULL, "Ignore different cases when comparing letters") \
    X(unique, "unique", 'u', 0, NULL, "Only print unique lines") \
    X(check_chars, "check-chars", 'w', 1, "Number of chars", "Compare no more then N characters in a line") \
    X(skip_chars, "skip-chars", 's', 1, "Number of chars", "Skip N characters at the beginning of the line") \
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

enum disp_type {
    DISP_DEFAULT,
    DISP_REPEAT,
    DISP_UNIQUE
} disp_type = DISP_DEFAULT;

static int disp_count = 0, ignore_case = 0;
static int check_chars = 0, skip_chars = 0;

static int cur_count = 0;

int compare_strings(const char *s1, const char *s2)
{
    if (s1 == NULL || s2 == NULL)
        return 1;

    if (check_chars == 0) {
        if (!ignore_case)
            return strcmp(s1, s2);
        else
            return stringcasecmp(s1, s2);
    } else {
        if (!ignore_case)
            return strncmp(s1, s2, check_chars);
        else
            return stringncasecmp(s1, s2, check_chars);
    }
}

void print_line(const char *line)
{
    if (disp_count)
        printf("%7d ", cur_count);

    printf("%s\n", line);
}

void display_file(FILE *file)
{
    int repeat_flag = 0;
    size_t curlen, otherlen = 0;
    char *curbuf = NULL, *otherbuf = NULL;
    int cmp;

    while ((curlen = getline(&curbuf, &curlen, file)) != -1) {
        char *curbuf_s = curbuf, *otherbuf_s = otherbuf;

        curbuf[curlen - 1] = '\0';
        curlen--;

        if (otherbuf != NULL) {
            if (skip_chars) {
                if (curlen > skip_chars)
                    curbuf_s += skip_chars;
                else
                    curbuf_s += curlen;

                if (otherlen > skip_chars)
                    otherbuf_s += skip_chars;
                else
                    otherbuf_s += otherlen;
            }

            cmp = compare_strings(curbuf_s, otherbuf_s);
        } else {
            cmp = 1;
        }

        switch (disp_type) {
        case DISP_DEFAULT:
            if (cmp != 0 && otherbuf)
                print_line(otherbuf);

            break;
        case DISP_REPEAT:
            if (cmp == 0)
                print_line(curbuf);

            break;
        case DISP_UNIQUE:
            if (cmp == 0) {
                repeat_flag = 1;
            } else {
                if (!repeat_flag && otherbuf)
                    print_line(otherbuf);
                else
                    repeat_flag = 0;
            }
            break;
        }

        if (cmp == 0)
            cur_count++;
        else
            cur_count = 1;


        if (cmp == 0) {
            free(curbuf);
            curbuf = NULL;
            curlen = 0;
        } else {
            free(otherbuf);
            otherbuf = curbuf;
            otherlen = curlen;
            curbuf = NULL;
            curlen = 0;
        }
    }

    switch (disp_type) {
    case DISP_DEFAULT:
        print_line(otherbuf);

        break;
    case DISP_UNIQUE:
        if (repeat_flag == 0)
            print_line(otherbuf);

        break;
    case DISP_REPEAT:
        if (cmp == 0)
            print_line(otherbuf);

        break;
    }

    free(curbuf);
    free(otherbuf);
}

int main(int argc, char **argv) {
    FILE *file;
    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            break;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_count:
            disp_count = 1;
            break;
        case ARG_repeated:
            disp_type = DISP_REPEAT;
            break;
        case ARG_ignore_case:
            ignore_case = 1;
            break;
        case ARG_unique:
            disp_type = DISP_UNIQUE;
            break;
        case ARG_check_chars:
            check_chars = strtol(argarg, NULL, 0);
            break;
        case ARG_skip_chars:
            skip_chars = strtol(argarg, NULL, 0);
            break;

        case ARG_EXTRA:
            file = fopen_with_dash(argarg, "r");
            if (file == NULL) {
                perror(argarg);
                return 1;
            }
            display_file(file);
            fclose_with_dash(file);
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    return 0;
}

