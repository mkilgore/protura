#ifndef DSH_INPUT_LEXER_H
#define DSH_INPUT_LEXER_H

#include <stdio.h>

enum input_token {
    TOK_STRING,
    TOK_REDIRECT_IN,
    TOK_REDIRECT_OUT,
    TOK_REDIRECT_APPEND_OUT,
    TOK_PIPE,
    TOK_BACKGROUND,
    TOK_LOGIC_AND,
    TOK_LOGIC_OR,
    TOK_COMMENT,
    TOK_NEWLINE,
    TOK_EOF,
    TOK_UNKNOWN,
};

struct input_lexer {
    const char *input;
    size_t location;

    int line;
    const char *str;
    size_t len;
};

enum input_token lexer_next_token(struct input_lexer *);


/*
enum input_token yylex(struct input_lexer *);
extern FILE *yyin;
extern char *yytext;
extern int yylex_destroy(void);

struct yy_buffer_state;

extern struct yy_buffer_state *yy_scan_string(const char *string);
extern void yy_delete_buffer(struct yy_buffer_state *buffer);
*/

#endif
