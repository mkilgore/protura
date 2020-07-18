#ifndef INCLUDE_UAPI_PROTURA_EVENT_PROTOCOL_H
#define INCLUDE_UAPI_PROTURA_EVENT_PROTOCOL_H

#include <protura/types.h>

enum {
    KERN_EVENT_KEYBOARD,
};

struct kern_event {
    __kuint16_t type;
    __kuint16_t code;
    __kuint32_t value;
};

#endif
