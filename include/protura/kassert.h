#ifndef INCLUDE_PROTURA_KASSERT_H
#define INCLUDE_PROTURA_KASSERT_H

#include <protura/compiler.h>
#include <protura/stddef.h>
#include <protura/debug.h>

#define BUG(str, ...) \
    do { \
        kp(KP_ERROR, "BUG: %s: %d/%s(): \"%s\"\n", __FILE__, __LINE__, __func__, str); \
        panic(__VA_ARGS__); \
    } while (0)

/* Called with 'condition' and then printf-like format string + arguments */
#define kassert(cond, ...) \
    do { \
        int __kassert_cond = (cond); \
        if (unlikely(!__kassert_cond)) \
            BUG(Q(cond), __VA_ARGS__); \
    } while (0)

#endif
