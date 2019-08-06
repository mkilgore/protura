// stty - Print configuration information for the a tty
#define UTILITY_NAME "stty"

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "tables.h"

#define CC_ENTRY(nam, idx) \
    { .index = (idx), .name = (nam) }

struct cc_entry cc_entries[] = {
    CC_ENTRY("intr",    VINTR),
    CC_ENTRY("quit",    VQUIT),
    CC_ENTRY("erase",   VERASE),
    CC_ENTRY("kill",    VKILL),
    CC_ENTRY("eof",     VEOF),
    CC_ENTRY("switch",  VSWTC),
    CC_ENTRY("start",   VSTART),
    CC_ENTRY("stop",    VSTOP),
    CC_ENTRY("susp",    VSUSP),
    CC_ENTRY("eol",     VEOL),
    CC_ENTRY("rprnt",   VREPRINT),
    CC_ENTRY("discard", VDISCARD),
    CC_ENTRY("werase",  VWERASE),
    CC_ENTRY("lnext",   VLNEXT),
    CC_ENTRY("eol2",    VEOL2),
    CC_ENTRY("time",    VTIME),
    CC_ENTRY("min",     VMIN),
    { .name = NULL },
};

#define OFF 0
#define ON 1

#define FLAG_ENTRY(nam, flg, typ, d) \
    { .flag = (flg), .name = (nam), .type = (typ), .def = (d) }

struct flag_entry flag_entries[] = {
    FLAG_ENTRY("ignbrk", IGNBRK, FLAG_INPUT,   OFF),
    FLAG_ENTRY("brkint", BRKINT, FLAG_INPUT,   OFF),
    FLAG_ENTRY("ignpar", IGNPAR, FLAG_INPUT,   OFF),
    FLAG_ENTRY("parmrk", PARMRK, FLAG_INPUT,   OFF),
    FLAG_ENTRY("inpck",  INPCK,  FLAG_INPUT,   OFF),
    FLAG_ENTRY("istrip", ISTRIP, FLAG_INPUT,   OFF),
    FLAG_ENTRY("inlcr",  INLCR,  FLAG_INPUT,   OFF),
    FLAG_ENTRY("icrnl",  ICRNL,  FLAG_INPUT,   ON),
    FLAG_ENTRY("igncr",  IGNCR,  FLAG_INPUT,   OFF),
    FLAG_ENTRY("iuclc",  IUCLC,  FLAG_INPUT,   OFF),
    FLAG_ENTRY("ixon",   IXON,   FLAG_INPUT,   ON),
    FLAG_ENTRY("ixoff",  IXOFF,  FLAG_INPUT,   OFF),
    FLAG_ENTRY("ixany",  IXANY,  FLAG_INPUT,   OFF),
    FLAG_ENTRY("iutf8",  IUTF8,  FLAG_INPUT,   ON),
    FLAG_ENTRY("opost",  OPOST,  FLAG_OUTPUT,  ON),
    FLAG_ENTRY("olcuc",  OLCUC,  FLAG_OUTPUT,  OFF),
    FLAG_ENTRY("onlcr",  ONLCR,  FLAG_OUTPUT,  ON),
    FLAG_ENTRY("ocrnl",  OCRNL,  FLAG_OUTPUT,  OFF),
    FLAG_ENTRY("onocr",  ONOCR,  FLAG_OUTPUT,  OFF),
    FLAG_ENTRY("onlret", ONLRET, FLAG_OUTPUT,  OFF),
    FLAG_ENTRY("ofill",  OFILL,  FLAG_OUTPUT,  OFF),
    FLAG_ENTRY("ofdel",  OFDEL,  FLAG_OUTPUT,  OFF),
    FLAG_ENTRY("cstopb", CSTOPB, FLAG_CONTROL, OFF),
    FLAG_ENTRY("cread",  CREAD,  FLAG_CONTROL, ON),
    FLAG_ENTRY("parenb", PARENB, FLAG_CONTROL, OFF),
    FLAG_ENTRY("parodd", PARODD, FLAG_CONTROL, OFF),
    FLAG_ENTRY("hupcl",  HUPCL,  FLAG_CONTROL, OFF),
    FLAG_ENTRY("clocal", CLOCAL, FLAG_CONTROL, OFF),
    FLAG_ENTRY("isig",   ISIG,   FLAG_LINE,    ON),
    FLAG_ENTRY("icanon", ICANON, FLAG_LINE,    ON),
    FLAG_ENTRY("echo",   ECHO,   FLAG_LINE,    ON),
    FLAG_ENTRY("echoe",  ECHOE,  FLAG_LINE,    ON),
    FLAG_ENTRY("echok",  ECHOK,  FLAG_LINE,    ON),
    FLAG_ENTRY("echonl", ECHONL, FLAG_LINE,    OFF),
    FLAG_ENTRY("noflsh", NOFLSH, FLAG_LINE,    OFF),
    FLAG_ENTRY("tostop", TOSTOP, FLAG_LINE,    OFF),
    FLAG_ENTRY("iexten", IEXTEN, FLAG_LINE,    ON),
    { .name = NULL },
};

/* buf must be at least 4 characters */
void render_key(char key, char *buf)
{
    switch (key)
    {
    case 0 ... 26:
        buf[0] = '^';
        buf[1] = 'A'  + key - 1;
        buf[2] = '\0';
        return;

    case 0x7F:
        buf[0] = '^';
        buf[1] = '?';
        buf[2] = '\0';
        return;

    default:
        buf[0] = key;
        buf[1] = '\0';
        return;
    }
}

void termios_get_flag(struct termios *termios, enum flag_type type, tcflag_t **flag_value)
{
    switch (type) {
    case FLAG_INPUT:
        *flag_value = &termios->c_iflag;
        break;

    case FLAG_OUTPUT:
        *flag_value = &termios->c_oflag;
        break;

    case FLAG_CONTROL:
        *flag_value = &termios->c_cflag;
        break;

    case FLAG_LINE:
        *flag_value = &termios->c_lflag;
        break;
    }
}
