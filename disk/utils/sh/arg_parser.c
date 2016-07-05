/*
 * Copyright (C) 2013 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "arg_parser.h"

const void *argarg;

static int current_arg = 1;
static const char *shrt;
static int parsing_done;

int arg_parser(int parser_argc, char **parser_argv, const struct arg *cmips_args)
{
    const char *cur;
    if (current_arg == parser_argc)
        return ARG_PARSER_DONE;

    cur = parser_argv[current_arg];

    if (parsing_done)
        goto parsing_done;

    if (cur[0] == '-' && !shrt) {
        if (*(cur + 1) && cur[1] == '-') {
            int i;

            if (!cur[2]) {
                parsing_done = 1;
                current_arg++;
                goto parsing_done;
            }


            for (i = 0; cmips_args[i].lng != NULL; i++) {
                if (strcmp(cmips_args[i].lng, cur + 2) == 0) {
                    if (cmips_args[i].has_arg) {
                        if (parser_argc == current_arg + 1) {
                            printf("%s: Not enough arguments to '%s'\n", parser_argv[0], cur);
                            return ARG_PARSER_ERR;
                        }
                        argarg = parser_argv[current_arg + 1];
                        current_arg++;
                    }

                    return i;
                }
            }

            printf("%s: unreconized argument '%s'\n", parser_argv[0], cur);
            current_arg++;
            return ARG_PARSER_ERR;
        }

        shrt = cur + 1;
    }

    if (shrt) {
        int i;
        for (i = 0; cmips_args[i].lng != NULL; i++) {
            if (cmips_args[i].shrt == *shrt) {
                shrt++;
                if (!*shrt) {
                    shrt = NULL;
                    current_arg++;
                }
                if (cmips_args[i].has_arg) {
                    if (shrt) {
                        argarg = shrt;
                        shrt = NULL;
                        current_arg++;
                    } else {
                        if (parser_argc == current_arg) {
                            printf("%s: Not enough arguments to '-%c'\n", parser_argv[0], cmips_args[i].shrt);
                            return ARG_PARSER_ERR;
                        }
                        argarg = parser_argv[current_arg];
                        current_arg++;
                    }
                }
                return i;
            }
        }

        printf("%s: unreconized argument '-%c'\n", parser_argv[0], *shrt);
        return ARG_PARSER_ERR;
    }

parsing_done:

    argarg = parser_argv[current_arg];
    current_arg++;
    return ARG_PARSER_EXTRA;
}

void display_help_text(const char *prog, const struct arg *cmips_args)
{
    const struct arg *a;
    printf("Usage: %s [Flags] [Files] \n"
           "\n"
           "Files: Assembly source files to load on startup\n"
           "Flags:\n", prog);

    for (a = cmips_args; a->lng != NULL; a++) {
        printf("  ");
        if (a->shrt != 0)
            printf("-%c, ", a->shrt);
        else
            printf("     ");

        printf("--%-15s %s\n", a->lng, a->help_txt);
    }

    printf("See the manpage for more information\n");
}


