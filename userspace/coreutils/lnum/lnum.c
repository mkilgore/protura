// lnum - prepend line numbers to the given input (stdin by default)
//      - intended to be used as a filter.
#define UTILITY_NAME "lnum"

#include "common.h"

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "arg_parser.h"
#include "stringcasecmp.h"
#include "file.h"

#define MAX_LINE_SIZE 1000

static const char *arg_str = "[Flags] [Files]";
static const char *usage_str = "Prepend line numbers to Files (standard in by default\n";
static const char *arg_desc_str  = "Files: List of files to output with line-numbers added.\n";

#define XARGS \
    X(help, "help", 'h', 0, NULL, "Display help") \
    X(version, "version", 'v', 0, NULL, "Display version information") \
    X(delimiter, "delimiter", 'd', 1, "String", "Use the string provided to the delimiter flag instead of a tab") \
    X(numwidth, "width", 'w', 1, "Int", "Specify the width to pad the line number too - The default is no padding") \
    X(numalign, "align", 'a', 1, "[left|right]", "Specify the alignment of the line number, 'left' or 'right'. Default is left") \
    X(last, NULL, '\0', 0, NULL, NULL)

enum arg_index {
  ARG_EXTRA = ARG_PARSER_EXTRA,
  ARG_ERR = ARG_PARSER_ERR,
  ARG_DONE = ARG_PARSER_DONE,
#define X(enu, ...) ARG_ENUM(enu)
  XARGS
#undef X
};

static const struct arg lnum_args[] = {
#define X(...) CREATE_ARG(__VA_ARGS__)
  XARGS
#undef X
};


void show_line_numbers(FILE *in);

static const char *delimiter = "\t";
static int num_width_pad = 0;

static enum num_align {
    ALIGN_LEFT,
    ALIGN_RIGHT
} num_alignment = ALIGN_LEFT;

int main(int argc, char **argv) {
  bool had_files = false;
  FILE *file;
  enum arg_index ret;

  while ((ret = arg_parser(argc, argv, lnum_args)) != ARG_DONE) {
    switch (ret) {
    case ARG_help:
      display_help_text(argv[0], arg_str, usage_str, arg_desc_str, lnum_args);
      return 0;
    case ARG_version:
      printf("%s", version_text);
      return 0;
    case ARG_delimiter:
      delimiter = argarg;
      break;
    case ARG_numwidth:
      num_width_pad = strtol(argarg, NULL, 0);
      break;
    case ARG_numalign:
      num_alignment = (stringcasecmp(argarg, "left") == 0)? ALIGN_LEFT: ALIGN_RIGHT;
      break;
    case ARG_EXTRA:
      had_files = true;
      file = fopen_with_dash(argarg, "r");
      if (file == NULL) {
        perror(argarg);
        return 1;
      }
      show_line_numbers(file);
      fclose_with_dash(file);
      break;
    default:
      return 0;
    }
  }

  if (!had_files) {
    show_line_numbers(stdin);
  }

  return 0;
}

void show_line_numbers(FILE *in) {
  static int line_count = 1; /* Static, so the lines continue over multiple files */
  char line[MAX_LINE_SIZE];

  while ( fgets(line, MAX_LINE_SIZE, in) ) {
    printf((num_alignment == ALIGN_LEFT)? "%-*d%s%s": "%*d%s%s", num_width_pad, line_count, delimiter, line);
    line_count++;
  }
}

