#ifndef _INCLUDE_DISPLAY
#define _INCLUDE_DISPLAY

#include <termios.h>
#include "tables.h"

enum disp_type {
    DISP_DIFF,
    DISP_ALL,
};

void print_cc(struct termios *termios);
void print_winsize(struct winsize *wins);
int print_flag(struct termios *termios, struct flag_entry *entry);
void print_flags(struct termios *termios, struct flag_entry *entries, enum disp_type);
void display_termios(struct termios *termios, enum disp_type);

#endif
