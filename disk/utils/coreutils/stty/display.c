// stty - Print configuration information for the a tty
#define UTILITY_NAME "stty"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "tables.h"
#include "display.h"

void print_cc(struct termios *termios)
{
    char buf[4];
    int line_len = 0;

    struct cc_entry *entry = cc_entries;
    for (; entry->name; entry++) {
        render_key(termios->c_cc[entry->index], buf);
        line_len += printf("%s = %s; ", entry->name, buf);

        if (line_len > 70) {
            putchar('\n');
            line_len = 0;
        }
    }

    if (line_len)
        putchar('\n');
}

void print_winsize(struct winsize *wins)
{
    printf("rows %d; columsn %d;\n", wins->ws_row, wins->ws_col);
}

static int get_flag_status(struct termios *termios, struct flag_entry *entry)
{
    tcflag_t *flag;
    termios_get_flag(termios, entry->type, &flag);

    int s = !!(*flag & entry->flag);
    return s;
}

int print_flag(struct termios *termios, struct flag_entry *entry)
{
    return printf("%s%s ", get_flag_status(termios, entry)? "": "-", entry->name);
}

void print_flags(struct termios *termios, struct flag_entry *entries, enum disp_type disp)
{
    int line_length = 0;
    struct flag_entry *entry = entries;

    for (; entry->name; entry++) {
        if (disp == DISP_DIFF)
            if (get_flag_status(termios, entry) == entry->def)
                continue;

        line_length += print_flag(termios, entry);

        if (line_length > 70) {
            putchar('\n');
            line_length = 0;
        }
    }

    if (line_length)
        putchar('\n');
}

void display_termios(struct termios *termios, enum disp_type disp)
{
    print_cc(termios);
    print_flags(termios, flag_entries, disp);
}

