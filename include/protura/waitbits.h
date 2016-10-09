/*
 * Copyright (C) 2016 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_WAITBITS_H
#define INCLUDE_PROTURA_WAITBITS_H

/*
 * Status is in the format:
 * <info byte> <code byte>
 *
 * code 0:         Child exited normally, info is return code.
 * code 1 to 0x7e: Child exited due to signal, code is signal number.
 * code 0x7f:      Code is stopped, info is signal number
 */


#define __kWNOHANG    1
#define __kWUNTRACED  2
#define __kWSTOPPED   __kWUNTRACED
#define __kWCONTINUED 8

#define __kWCONTINUEDBITS 0xFFFF

#define __kWIFEXITED(status)    (((status) & 0xFF) == 0)
#define __kWIFSIGNALED(status)  (((status) & 0x7f) > 0 && (((status) & 0x7f) < 0x7f))
#define __kWIFSTOPPED(status)   (((status) & 0xff) == 0x7F)

#define __kWEXITSTATUS(status)  (((status) >> 8) & 0xFF)
#define __kWTERMSIG(status)     ((status) & 0x7F)
#define __kWSTOPSIG(status)     (((status) >> 8) & 0xFF)

#define __kWIFCONTINUED(status) ((status) == __kWCONTINUEDBITS)

#endif
