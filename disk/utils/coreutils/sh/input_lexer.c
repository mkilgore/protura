/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

// sh - shell, command line interpreter
#define UTILITY_NAME "sh"

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "shell.h"
#include "input_lexer.h"

char *lexer_input_replace_env(const char *line)
{
    char *new_line = NULL;

    if (!strchr(line, '$'))
        return NULL;

    char *c;
    char *dupline = strdup(line);
    char *start = dupline;

    while ((c = strchr(dupline, '$')) != NULL) {
        *c = '\0';
        c++;

        /* Append string before '$' */
        a_sprintf_append(&new_line, "%s", start);

        /* Find end of environment variable */
        char *end = c;
        while (*end && ((*end >= 'a' && *end <= 'z')
               || (*end >= 'A' && *end <= 'Z')
               || *end == '_'))
            end++;

        /* If there's no environment variable name after the $, then we leave
         * the dollar sign alone. */
        if (end == c) {
            a_sprintf_append(&new_line, "$");
            start = c;
            continue;
        }

        /* Record the character after the end of the environment variable
         * and then mark it with `'\0' so we can pass it to getenv */
        char saved = *end;
        *end = '\0';

        char *env = getenv(c);

        if (env) {
            a_sprintf_append(&new_line, "%s", env);
        } else {
            char *endp = NULL;
            int arg_num = strtol(c, &endp, 10);

            /* If the string wasn't empty, and we read the whole string */
            if (endp && !*endp
                && arg_num >= 0 && arg_num < sh_argc) {
                a_sprintf_append(&new_line, "%s", sh_args[arg_num]);
            }
        }

        /* If the char after the env variable wasn't '\0', then print it and
         * set start after that value */
        if (saved) {
            a_sprintf_append(&new_line, "%c", saved);

            start = end + 1;
        } else {
            start = end;
        }
    }

    if (new_line)
        if (*start)
            a_sprintf_append(&new_line, "%s", start);

    free(dupline);
    return new_line;
}

enum input_token lexer_next_token(struct input_lexer *lex)
{
    char tok;

    /* Swallow any leading white-space */
    while (tok = lex->input[lex->location],
           tok == ' ' || tok == '\t')
        lex->location++;

    if (lex->input[lex->location] == '\0')
        return TOK_EOF;

    lex->str = lex->input + lex->location;

    switch (lex->input[lex->location++]) {
    case '#':
        lex->len = 1;
        return TOK_COMMENT;

    case '<':
        lex->len = 1;
        return TOK_REDIRECT_IN;

    case '>':
        if (lex->input[lex->location] == '>') {
            lex->location++;
            lex->len++;
            return TOK_REDIRECT_APPEND_OUT;
        }
        lex->len = 1;
        return TOK_REDIRECT_OUT;

    case '&':
        if (lex->input[lex->location] != '&') {
            lex->len = 1;
            return TOK_BACKGROUND;
        }

        lex->location++;
        lex->len = 2;
        return TOK_LOGIC_AND;

    case '|':
        if (lex->input[lex->location] != '|') {
            lex->len = 1;
            return TOK_PIPE;
        }

        lex->location++;
        lex->len = 2;
        return TOK_LOGIC_OR;

    case '\n':
        lex->line++;
        lex->len = 1;
        return TOK_NEWLINE;

    /* "string" case */
    case '\"':
        lex->len = 0;
        lex->str++;

        while (tok = lex->input[lex->location],
               tok != '\"') {
            lex->len++;
            lex->location++;
        }

        lex->location++;

        return TOK_STRING;

    /* Everything else is considered part of a string */
    default:
        lex->len = 1;
        while (tok = lex->input[lex->location],
               (tok >= 'a' && tok <= 'z')
               || (tok >= 'A' && tok <= 'Z')
               || (tok >= '0' && tok <= '9')
               || tok == '_'
               || tok == '.'
               || tok == '/'
               || tok == '-'
               || tok == ':'
               || tok == '$'
               || tok == '=') {
            lex->len++;
            lex->location++;
        }

        return TOK_STRING;
    }
}

