#ifndef INCLUDE_COMMON_H
#define INCLUDE_COMMON_H

#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

/* Inspired via the Linux-kernel macro 'container_of' */
#define container_of(ptr, type, member) \
    ((type *) ((char*)(ptr) - offsetof(type, member)))

#define ARRAY_SIZE(arr) (sizeof(arr)/sizeof(*arr))

#define QQ(x) #x
#define Q(x) QQ(x)

#define TP2(x, y) x ## y
#define TP(x, y) TP2(x, y)

#ifndef UTILITY_NAME
# error "Please define UTILITY_NAME macro before including common.h!"
#endif

#define IU_VERSION_MAJOR 0
#define IU_VERSION_MINOR 1
#define IU_VERSION IU_VERSION_MAJOR.IU_VERSION_MINOR

#define version_text (UTILITY_NAME " (iu-coreutils) " Q(IU_VERSION) "\n")

#if 0
static const char *version_text =
    UTILITY_NAME " (iu-coreutils) " Q(IU_VERSION) "\n";
#endif

#endif
