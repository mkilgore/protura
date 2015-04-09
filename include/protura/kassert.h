#ifndef INCLUDE_PROTURA_KASSERT_H
#define INCLUDE_PROTURA_KASSERT_H

#include <protura/compiler.h>
#include <protura/kprintf.h>
#include <protura/stddef.h>
#include <protura/debug.h>

#define BUG(str) \
    do { \
        kprintf("BUG: at %s:%d/%s(): %s\n", __FILE__, __LINE__, __func__, str); \
        panic(); \
    } while (0)

#define kassert(cond) \
    do { \
        int __kassert_cond = (cond); \
        if (unlikely(__kassert_cond)) \
            BUF(QQ(QQ(cond))); \
    } while (0)

#endif
