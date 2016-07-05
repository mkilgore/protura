/*
 * Copyright (C) 2013 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef COMMON_ARG_PARSER_H
#define COMMON_ARG_PARSER_H

struct arg {
    const char *lng;
    char shrt;
    const char *help_txt;

    int has_arg :1;
};

#define ARG_PARSER_EXTRA -4
#define ARG_PARSER_LNG -3
#define ARG_PARSER_ERR -2
#define ARG_PARSER_DONE -1

int arg_parser(int parser_argc, char **parser_argv, const struct arg *args);
void display_help_text(const char *prog, const struct arg *cmips_args);

extern const void *argarg;

#endif
