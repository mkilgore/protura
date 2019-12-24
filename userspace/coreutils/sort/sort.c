// sort - Sort lines of Files
#define UTILITY_NAME "sort"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arg_parser.h"
#include "file.h"

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Sort lines of Files\n";
static const char *arg_desc_str  = "Files: List of files to sort the contents of. '-' is standard input.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(numeric, "numeric", 'n', 0, NULL, "Do a numeric sort on each line") \
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

char **lines = NULL;
size_t line_count = 0;

int linecmp(const void *l1, const void *l2)
{
    const char *const *line1 = l1;
    const char *const *line2 = l2;
    return strcmp(*line1, *line2);
}

int numericcmp(const void *l1, const void *l2)
{
    const char *const *line1 = l1;
    const char *const *line2 = l2;
    char *l1_end, *l2_end;
    long n1, n2;

    n1 = strtol(*line1, &l1_end, 0);
    n2 = strtol(*line2, &l2_end, 0);

    if (l1_end == *line1 && l2_end == *line2) {
        return strcmp(*line1, *line2);
    } else if (l1_end == *line1) {
        return -1;
    } else if (l2_end == *line2) {
        return 1;
    } else {
        if (n1 > n2)
            return 1;
        else if (n1 < n2)
            return -1;
        else
            return 0;
    }
}

void add_lines(FILE *file)
{
    size_t len = 0;
    do {
        if (line_count > 0 && len != 0)
            lines[line_count - 1][len - 1] = '\0';

        lines = realloc(lines, sizeof (*lines) * (line_count + 1));
        lines[line_count] = NULL;
        len = 0;

    } while ((len = getline(lines + line_count++, &len, file)) != -1);
    free(lines[line_count - 1]);
    line_count -= 1;
}

int main(int argc, char **argv) {
    enum arg_index ret;
    FILE *file = NULL;
    int (*sortfunc) (const void *, const void *) = linecmp;
    int i;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_numeric:
            sortfunc = numericcmp;
            break;

        case ARG_EXTRA:
            file = fopen_with_dash(argarg, "r");
            add_lines(file);
            fclose_with_dash(file);
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!file)
        add_lines(stdin);

    qsort(lines, line_count, sizeof(*lines), sortfunc);

    for (i = 0; i < line_count; i++) {
        puts(lines[i]);
        free(lines[i]);
    }

    free(lines);

    return 0;
}

