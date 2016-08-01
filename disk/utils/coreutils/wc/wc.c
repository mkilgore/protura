// wc - Count newline, word, and bytes in file(s)
#define UTILITY_NAME "wc"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "arg_parser.h"

#define MAX_FILES 1024

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Count newlines, words, and bytes in Files\n";
static const char *arg_desc_str  = "Files: List of files to print statistics on. '-' represents stdin.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(bytes, "bytes", 'c', 0, NULL, "Print the byte count") \
    X(lines, "lines", 'l', 0, NULL, "Print the line count") \
    X(words, "words", 'w', 0, NULL, "Print the word count") \
    X(max_line, "max-line-length", 'L', 0, NULL, "Print the length of the longest line") \
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

static bool dsp_bytes = false, dsp_words = false;
static bool dsp_lines = false, dsp_max_line_len = false;

struct file_count {
    const char *filename;
    int bytes, lines, words, max_line;
};

static struct file_count total_count = { "total", 0 };
static struct file_count files[MAX_FILES] = { { 0 } };

static int dsp_file_count = 0, max_digits = 1;

static bool longest_in_left = false;

int count_digits(uint64_t value);
int count_file(int file_index);
void print_file_count(struct file_count *);

int main(int argc, char **argv) {
    int i, print_first_index;
    enum arg_index ret;

    while ((ret = arg_parser(argc, argv, args)) != ARG_DONE) {
        switch (ret) {
        case ARG_help:
            display_help_text(argv[0], arg_str, usage_str, arg_desc_str, args);
            return 0;
        case ARG_version:
            printf("%s", version_text);
            return 0;

        case ARG_bytes:
            dsp_bytes = true;
            break;

        case ARG_words:
            dsp_words = true;
            break;

        case ARG_lines:
            dsp_lines = true;
            break;

        case ARG_max_line:
            dsp_max_line_len = true;
            break;

        case ARG_EXTRA:
            if (dsp_file_count == MAX_FILES) {
                printf("%s: Error, maximum number of files is %d\n", argv[0], MAX_FILES);
                return 1;
            }
            files[dsp_file_count++].filename = argarg;
            break;

        case ARG_ERR:
        default:
            return 0;
        }
    }

    if (!dsp_bytes && !dsp_words && !dsp_lines && !dsp_max_line_len) {
        dsp_bytes = true;
        dsp_words = true;
        dsp_lines = true;
    }

    if (dsp_lines)
        print_first_index = 0;
    else if (dsp_words)
        print_first_index = 1;
    else if (dsp_bytes)
        print_first_index = 2;
    else if (dsp_max_line_len)
        print_first_index = 3;

    if (dsp_file_count == 0)
        files[dsp_file_count++].filename = "-";

    for (i = 0; i < dsp_file_count; i++) {
        uint64_t values[4] = { 0 };
        int k;

        if (count_file(i) != 0)
            return 1;

        total_count.bytes += files[i].bytes;
        total_count.lines += files[i].lines;
        total_count.words += files[i].words;

        if (total_count.max_line < files[i].max_line)
            total_count.max_line = files[i].max_line;

        if (dsp_lines)
            values[0] = files[i].lines;
        if (dsp_words)
            values[1] = files[i].words;
        if (dsp_bytes)
            values[2] = files[i].bytes;
        if (dsp_max_line_len)
            values[3] = files[i].max_line;

        for (k = 0; k < sizeof(values)/sizeof(values[0]); k++) {
            int digits = count_digits(values[k]);
            if (max_digits < digits) {
                if (k == print_first_index)
                    longest_in_left = true;
                else
                    longest_in_left = false;
                max_digits = digits;
            }
        }
    }

    for (i = 0; i < dsp_file_count; i++)
        print_file_count(files + i);

    if (dsp_file_count > 1)
        print_file_count(&total_count);

    return 0;
}

int count_digits(uint64_t value) {
    int len = 0;
    if (value == 0)
        return 1;

    while (value) {
        value /= 10;
        len++;
    }
    return len;
}

int count_file(int file_index) {
    FILE *file;
    bool is_stdin = false;

    struct file_count *count = files + file_index;

    int ch;
    uint64_t line_len = 0;
    bool in_word = false;

    if (strcmp(count->filename, "-") != 0) {
        file = fopen(count->filename, "r");
        if (file == NULL) {
            perror(count->filename);
            return 1;
        }
    } else {
        is_stdin = true;
        file = stdin;
    }

    while ((ch = fgetc(file)) != EOF) {
        count->bytes++;
        line_len++;

        switch (ch) {
        case '\n':
            line_len--;
            if (line_len > count->max_line)
                count->max_line = line_len;
            line_len = 0;
            count->lines++;
            /* Fall through, '\n' is also white-space */

        case ' ':
        case '\t':
            in_word = false;
            break;

        default:
            if (!in_word) {
                in_word = true;
                count->words++;
            }
            break;
        }
    }

    if (!is_stdin)
        fclose(file);

    return 0;
}

void print_file_count(struct file_count *file) {
    if (longest_in_left)
        putchar(' ');

    if (dsp_lines)
        printf("%*d ", max_digits, file->lines);
    if (dsp_words)
        printf("%*d ", max_digits, file->words);
    if (dsp_bytes)
        printf("%*d ", max_digits, file->bytes);
    if (dsp_max_line_len)
        printf("%*d ", max_digits, file->max_line);
    printf("%s\n", file->filename);
}

