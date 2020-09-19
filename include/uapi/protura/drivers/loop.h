#ifndef INCLUDE_UAPI_PROTURA_DRIVERS_LOOP_H
#define INCLUDE_UAPI_PROTURA_DRIVERS_LOOP_H

#include <protura/types.h>

#define LOOP_NAME_MAX 120

struct loopctl_create {
    __kuint32_t loop_number; /* Returned */
    __kuint32_t fd;
    char loop_name[LOOP_NAME_MAX];
};

struct loopctl_destroy {
    __kuint32_t loop_number;
};

struct loopctl_status {
    __kuint32_t loop_number;
    char loop_name[LOOP_NAME_MAX];
};

#define __LOOPIO 90

#define LOOPCTL_CREATE ((__LOOPIO << 8) + 0)
#define LOOPCTL_DESTROY ((__LOOPIO << 8) + 1)
#define LOOPCTL_STATUS  ((__LOOPIO << 8) + 2)

#endif
