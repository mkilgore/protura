#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "arg_parser.h"

char *argarg;
int current_arg = 1;

static char *shrt;
static int parsing_done;

static int load_extra_arg(char **parser_argv) {
    argarg = parser_argv[current_arg];
    current_arg++;
    return ARG_PARSER_EXTRA;
}

static int handle_long_arg(int parser_argc, char **parser_argv, const struct arg *args, char *cur) {
    int i;

    for (i = 0; args[i].lng || args[i].shrt; i++) {
        if (!args[i].lng)
            continue;

        if (strcmp(args[i].lng, cur + 2) == 0) {
            current_arg++;
            if (args[i].has_arg) {
                if (parser_argc == current_arg) {
                    fprintf(stderr, "%s: Not enough arguments to '%s'\n", parser_argv[0], cur);
                    return ARG_PARSER_ERR;
                }
                argarg = parser_argv[current_arg];
                current_arg++;
            }

            return i;
        }
    }

    fprintf(stderr, "%s: unreconized argument '%s'\n", parser_argv[0], cur);
    current_arg++;
    return ARG_PARSER_ERR;
}

static int handle_short_arg(int parser_argc, char **parser_argv, const struct arg *args) {
    int i;
    for (i = 0; args[i].lng || args[i].shrt; i++) {
        if (!args[i].shrt)
            continue;

        if (args[i].shrt == *shrt) {
            shrt++;
            if (!*shrt) {
                shrt = NULL;
                current_arg++;
            }

            if (args[i].has_arg) {
                if (shrt) {
                    argarg = shrt;
                    shrt = NULL;
                    current_arg++;
                } else {
                    if (parser_argc == current_arg) {
                        fprintf(stderr, "%s: Not enough arguments to '-%c'\n", parser_argv[0], args[i].shrt);
                        return ARG_PARSER_ERR;
                    }
                    argarg = parser_argv[current_arg];
                    current_arg++;
                }
            }
            return i;
        }
    }

    fprintf(stderr, "%s: unreconized argument '-%c'\n", parser_argv[0], *shrt);
    return ARG_PARSER_ERR;
}

int arg_parser(int parser_argc, char **parser_argv, const struct arg *args) {
    char *cur;
    if (current_arg == parser_argc)
        return ARG_PARSER_DONE;

    cur = parser_argv[current_arg];

    if (parsing_done)
        return load_extra_arg(parser_argv);

    if (cur[0] == '-' && !shrt) {
        if (*(cur + 1) && cur[1] == '-') {

            /* Case of '--' arg
             * This is a special case that indicates every argument
             * afterward shouldn't be parsed for flags */
            if (!cur[2]) {
                parsing_done = 1;
                current_arg++;
                return load_extra_arg(parser_argv);
            }

            return handle_long_arg(parser_argc, parser_argv, args, cur);
        }

        shrt = cur + 1;

        if (*shrt == '\0')
            shrt = NULL;
    }

    if (shrt)
        return handle_short_arg(parser_argc, parser_argv, args);

    return load_extra_arg(parser_argv);
}

#define ARG_LEN 20

#define QQ(s) #s
#define Q(s) QQ(s)

void display_help_text(const char *prog, const char *arg_str, const char *usage, const char *arg_desc_str, const struct arg *args)
{
    const struct arg *a;
    printf("Usage: %s %s \n"
           "%s"
           "\n"
           "%s"
           "Flags:\n", prog, arg_str, usage, arg_desc_str);

    for (a = args; a->lng || a->shrt; a++) {
        printf("  ");
        if (a->shrt != '\0')
            printf("-%c%c ", a->shrt, a->lng? ',': ' ');
        else
            printf("     ");

        if (a->lng && !a->has_arg) {
            printf("--%-" Q(ARG_LEN) "s ", a->lng);
        } else if (a->lng) {
            int len = ARG_LEN - strlen(a->lng) - 1;
            printf("--%s=%-*s ", a->lng, len, a->arg_txt);
        } else {
            printf("  %" Q(ARG_LEN) "s ", "");
        }

        printf("%s\n", a->help_txt);
    }
}

