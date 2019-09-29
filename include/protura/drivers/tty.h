/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_TTY_H
#define INCLUDE_PROTURA_TTY_H

#if __KERNEL__
# include <protura/types.h>
# include <protura/list.h>
# include <protura/char_buf.h>
# include <protura/spinlock.h>
# include <protura/mutex.h>
# include <protura/work.h>
# include <protura/wait.h>
#endif

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS 19
struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_line;
    cc_t c_cc[NCCS];
};

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

/* c_cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSWTC 7
#define VSTART 8
#define VSTOP 9
#define VSUSP 10
#define VEOL 11
#define VREPRINT 12
#define VDISCARD 13
#define VWERASE 14
#define VLNEXT 15
#define VEOL2 16

/* c_iflag bits */
#define IGNBRK	0000001  /* Ignore break condition.  */ /* IGNORED */
#define BRKINT	0000002  /* Signal interrupt on break.  */ /* IGNORED */
#define IGNPAR	0000004  /* Ignore characters with parity errors.  */ /* IGNORED */
#define PARMRK	0000010  /* Mark parity and framing errors.  */ /* IGNORED */
#define INPCK	0000020  /* Enable input parity check.  */ /* IGNORED */
#define ISTRIP	0000040  /* Strip 8th bit off characters.  */ /* IGNORED */
#define INLCR	0000100  /* Map NL to CR on input.  */ /* IGNORED */
#define IGNCR	0000200  /* Ignore CR.  */ /* IGNORED */
#define ICRNL	0000400  /* Map CR to NL on input.  */ /* IGNORED */
#define IUCLC	0001000  /* Map uppercase characters to lowercase on input
			    (not in POSIX).  */ /* IGNORED */
#define IXON	0002000  /* Enable start/stop output control.  */ /* IGNORED */
#define IXANY	0004000  /* Enable any character to restart output.  */ /* IGNORED */
#define IXOFF	0010000  /* Enable start/stop input control.  */ /* IGNORED */
#define IMAXBEL	0020000  /* Ring bell when input queue is full
			    (not in POSIX).  */ /* IGNORED */
#define IUTF8	0040000  /* Input is UTF8 (not in POSIX).  */ /* IGNORED */

/* c_oflag bits */
#define OPOST	0000001  /* Post-process output.  */ /* IGNORED */
#define OLCUC	0000002  /* Map lowercase characters to uppercase on output.
			    (not in POSIX).  */ /* IGNORED */
#define ONLCR	0000004  /* Map NL to CR-NL on output.  */ /* IGNORED */
#define OCRNL	0000010  /* Map CR to NL on output.  */ /* IGNORED */
#define ONOCR	0000020  /* No CR output at column 0.  */ /* IGNORED */
#define ONLRET	0000040  /* NL performs CR function.  */ /* IGNORED */
#define OFILL	0000100  /* Use fill characters for delay.  */ /* IGNORED */
#define OFDEL	0000200  /* Fill is DEL.  */ /* IGNORED */


/* c_cflag bits.  */
#define CSIZE	0000060 /* IGNORED */
#define   CS5	0000000 /* IGNORED */
#define   CS6	0000020 /* IGNORED */
#define   CS7	0000040 /* IGNORED */
#define   CS8	0000060 /* IGNORED */
#define CSTOPB	0000100 /* IGNORED */
#define CREAD	0000200 /* IGNORED */
#define PARENB	0000400 /* IGNORED */
#define PARODD	0001000 /* IGNORED */
#define HUPCL	0002000 /* IGNORED */
#define CLOCAL	0004000 /* IGNORED */

/* c_lflag bits */
#define ISIG	0000001   /* Enable signals.  */ /* IGNORED */
#define ICANON	0000002   /* Canonical input (erase and kill processing).  */ /* IGNORED */
#define ECHO	0000010   /* Enable echo.  */ /* IGNORED */
#define ECHOE	0000020   /* Echo erase character as error-correcting backspace.  */ /* IGNORED */
#define ECHOK	0000040   /* Echo KILL.  */ /* IGNORED */
#define ECHONL	0000100   /* Echo NL.  */ /* IGNORED */
#define NOFLSH	0000200   /* Disable flush after interrupt or quit.  */ /* IGNORED */
#define TOSTOP	0000400   /* Send SIGTTOU for background output.  */ /* IGNORED */
#define IEXTEN	0100000   /* Enable implementation-defined input processing.  */ /* IGNORED */

/* c_cflag bit meaning */
#define  B0	0000000		/* hang up */
#define  B50	0000001
#define  B75	0000002
#define  B110	0000003
#define  B134	0000004
#define  B150	0000005
#define  B200	0000006
#define  B300	0000007
#define  B600	0000010
#define  B1200	0000011
#define  B1800	0000012
#define  B2400	0000013
#define  B4800	0000014
#define  B9600	0000015
#define  B19200	0000016
#define  B38400	0000017

#define  CBAUD  0000017

/* tcflow() and TCXONC use these */
#define	TCOOFF		0 /* IGNORED */
#define	TCOON		1 /* IGNORED */
#define	TCIOFF		2 /* IGNORED */
#define	TCION		3 /* IGNORED */

/* tcflush() and TCFLSH use these */
#define	TCIFLUSH	0 /* IGNORED */
#define	TCOFLUSH	1 /* IGNORED */
#define	TCIOFLUSH	2 /* IGNORED */

/* tcsetattr() uses these */
#define	TCSANOW		0 /* IGNORED */
#define	TCSADRAIN	1 /* IGNORED */
#define	TCSAFLUSH	2 /* IGNORED */

#define __TIO 90

#define TIOCGPGRP ((__TIO << 8) + 0)
#define TIOCSPGRP ((__TIO << 8) + 1)
#define TIOCGSID  ((__TIO << 8) + 2)
#define TCGETS    ((__TIO << 8) + 3)
#define TCSETS    ((__TIO << 8) + 4)
#define TCXONC    ((__TIO << 8) + 5)
#define TCFLSH    ((__TIO << 8) + 6)
#define TCSBRK    ((__TIO << 8) + 7)
#define TIOCGWINSZ ((__TIO << 8) + 8)
#define TIOCSWINSZ ((__TIO << 8) + 9)

#if __KERNEL__

extern struct file_ops tty_file_ops;
void tty_subsystem_init(void);

struct tty_driver;
struct tty;
struct file;

struct tty_ops {
    void (*register_for_wakeups) (struct tty *);
    int (*has_chars) (struct tty *);
    int (*read) (struct tty *, char *, size_t);
    int (*write) (struct tty *, const char *, size_t size);
};

struct tty_driver {
    const char *name;
    dev_t minor_start;
    dev_t minor_end;
    const struct tty_ops *ops;
};

struct tty {
    char *name;
    /* Device number for the driver Minor number is equal to this + minor_start */
    dev_t device_no;

    mutex_t lock;

    struct work work;

    pid_t session_id;
    pid_t fg_pgrp;

    struct winsize winsize;
    struct termios termios;
    struct tty_driver *driver;

    /* 
     * NOTE: This is labeled from the kernel's point of view.
     * Processes read from the 'output_buf'
     */
    struct wait_queue in_wait_queue;

    struct char_buf output_buf;
    int ret0; /* ^D marker */

    char *line_buf;
    size_t line_buf_pos;
    size_t line_buf_size;
};

void tty_pump(struct work *);

#define TTY_INIT(tty) \
    { \
        .lock = MUTEX_INIT((tty).lock, "tty-lock"), \
        .work = WORK_INIT_KWORK((tty).work, tty_pump), \
        .in_wait_queue = WAIT_QUEUE_INIT((tty).in_wait_queue, "tty-in-wait-queue"), \
    }

static inline void tty_init(struct tty *tty)
{
    *tty = (struct tty)TTY_INIT(*tty);
}

void tty_driver_register(struct tty_driver *driver);
int tty_write_buf(struct tty *tty, const char *buf, size_t len);

#define __TERMIOS_FLAG_OFLAG(termios, flag) (((termios)->c_oflag & (flag)) != 0)
#define __TERMIOS_FLAG_CFLAG(termios, flag) (((termios)->c_cflag & (flag)) != 0)
#define __TERMIOS_FLAG_LFLAG(termios, flag) (((termios)->c_lflag & (flag)) != 0)
#define __TERMIOS_FLAG_IFLAG(termios, flag) (((termios)->c_iflag & (flag)) != 0)

#define TERMIOS_OPOST(termios) __TERMIOS_FLAG_OFLAG((termios), OPOST)
#define TERMIOS_ISIG(termios) __TERMIOS_FLAG_LFLAG((termios), ISIG)
#define TERMIOS_ICANON(termios) __TERMIOS_FLAG_LFLAG((termios), ICANON)
#define TERMIOS_ECHO(termios) __TERMIOS_FLAG_LFLAG((termios), ECHO)
#define TERMIOS_ECHOE(termios) __TERMIOS_FLAG_LFLAG((termios), ECHOE)

#define TERMIOS_IGNBRK(termios) __TERMIOS_FLAG_IFLAG((termios), IGNBRK)
#define TERMIOS_ISTRIP(termios) __TERMIOS_FLAG_IFLAG((termios), ISTRIP)
#define TERMIOS_INLCR(termios)  __TERMIOS_FLAG_IFLAG((termios), INLCR)
#define TERMIOS_IGNCR(termios)  __TERMIOS_FLAG_IFLAG((termios), IGNCR)
#define TERMIOS_ICRNL(termios)  __TERMIOS_FLAG_IFLAG((termios), ICRNL)
#define TERMIOS_IUCLC(termios)  __TERMIOS_FLAG_IFLAG((termios), IUCLC)

#define TERMIOS_OLCUC(termios)  __TERMIOS_FLAG_OFLAG((termios), OLCUC)
#define TERMIOS_ONLCR(termios)  __TERMIOS_FLAG_OFLAG((termios), ONLCR)
#define TERMIOS_OCRNL(termios)  __TERMIOS_FLAG_OFLAG((termios), OCRNL)
#define TERMIOS_ONLRET(termios) __TERMIOS_FLAG_OFLAG((termios), ONLRET)

#endif

#endif
