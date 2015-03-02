#ifndef INCLUDE_ARCH_DRIVERS_SCANCODE_H
#define INCLUDE_ARCH_DRIVERS_SCANCODE_H

#include <protura/types.h>

extern uint16_t scancode2_to_ascii[][8];

enum {
    LED_NUM_LOCK = 2,
    LED_SCROLL_LOCK = 1,
    LED_CAPS_LOCK = 4,
};

enum {
    CK_SHIFT = 1,
    CK_ALT = 2,
    CK_CTRL = 4,
};

enum {
    KEY_CONTROL = 0x1D,

    KEY_LEFT_SHIFT = 0x2A,
    KEY_RIGHT_SHIFT = 0x36,

    KEY_ALT = 0x38,

    KEY_CAPS_LOCK = 0x3A,
    KEY_NUM_LOCK = 0x45,
    KEY_SCROLL_LOCK = 0x46,
};

#endif
