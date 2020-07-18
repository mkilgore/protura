/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef __INCLUDE_UAPI_PROTURA_DRIVERS_TTY_H__
#define __INCLUDE_UAPI_PROTURA_DRIVERS_TTY_H__

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
#define INLCR	0000100  /* Map NL to CR on input.  */
#define IGNCR	0000200  /* Ignore CR.  */
#define ICRNL	0000400  /* Map CR to NL on input.  */
#define IUCLC	0001000  /* Map uppercase characters to lowercase on input
			    (not in POSIX).  */
#define IXON	0002000  /* Enable start/stop output control.  */ /* IGNORED */
#define IXANY	0004000  /* Enable any character to restart output.  */ /* IGNORED */
#define IXOFF	0010000  /* Enable start/stop input control.  */ /* IGNORED */
#define IMAXBEL	0020000  /* Ring bell when input queue is full
			    (not in POSIX).  */ /* IGNORED */
#define IUTF8	0040000  /* Input is UTF8 (not in POSIX).  */ /* IGNORED */

/* c_oflag bits */
#define OPOST	0000001  /* Post-process output.  */
#define OLCUC	0000002  /* Map lowercase characters to uppercase on output.
			    (not in POSIX).  */
#define ONLCR	0000004  /* Map NL to CR-NL on output.  */
#define OCRNL	0000010  /* Map CR to NL on output.  */
#define ONOCR	0000020  /* No CR output at column 0.  */ /* IGNORED */
#define ONLRET	0000040  /* NL performs CR function.  */
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
#define ISIG	0000001   /* Enable signals.  */
#define ICANON	0000002   /* Canonical input (erase and kill processing).  */
#define ECHO	0000010   /* Enable echo.  */
#define ECHOE	0000020   /* Echo erase character as error-correcting backspace.  */
#define ECHOK	0000040   /* Echo KILL.  */ /* IGNORED */
#define ECHONL	0000100   /* Echo NL.  */ /* IGNORED */
#define ECHOCTL 0001000   /* Echo CTL */
#define NOFLSH	0000200   /* Disable flush after interrupt or quit.  */
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
#define	TCIFLUSH	0
#define	TCOFLUSH	1
#define	TCIOFLUSH	2

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

#define TIOSETKBD     ((__TIO << 8) + 127)
#define TIOSETCONSOLE ((__TIO << 8) + 128)

/* Sets whether keyboard input should be sent to the console.
 *
 * The expectation is that if you turn it off, you'll be listening for raw
 * keyboard input via the event interface. */
#define TTY_KEYBOARD_STATE_ON 0
#define TTY_KEYBOARD_STATE_OFF 1

#endif
