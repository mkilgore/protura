#ifndef COMMON_ARG_PARSER_H
#define COMMON_ARG_PARSER_H

struct arg {
  const char *lng;
  char shrt;
  const char *help_txt;
  const char *arg_txt;

  int has_arg :1;
};

#define ARG_PARSER_EXTRA -3
#define ARG_PARSER_ERR -2
#define ARG_PARSER_DONE -1

int arg_parser(int parser_argc, char **parser_argv, const struct arg *args);
void display_help_text(const char *prog, const char *arg_str, const char *usage, const char *arg_desc_str, const struct arg *args);

extern int current_arg;
extern char *argarg;

#define ARG_ENUM(enu) ARG_##enu,

#define ARG(id, op, arg, arg_text, help_text) \
  { .lng = id, \
    .shrt = op, \
    .help_txt = help_text, \
    .has_arg = arg, \
    .arg_txt = arg_text \
  }

#define CREATE_ARG(enu, id, op, arg, arg_text, help_text) \
  [ARG_##enu] = ARG(id, op, arg, arg_text, help_text),

#endif
