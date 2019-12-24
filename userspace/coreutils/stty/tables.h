#ifndef _INCLUDE_TABLES
#define _INCLUDE_TABLES

#include <termios.h>

struct cc_entry {
    int index;
    const char *name;
};

extern struct cc_entry cc_entries[];

enum flag_type {
    FLAG_CONTROL,
    FLAG_LINE,
    FLAG_OUTPUT,
    FLAG_INPUT,
};

struct flag_entry {
    tcflag_t flag;
    enum flag_type type;
    const char *name;
    int def;
};

extern struct flag_entry flag_entries[];

void render_key(char key, char *buf);
void termios_get_flag(struct termios *termios, enum flag_type type, tcflag_t **flag_value);

#endif
