/*
 * Copyright (C) 2014 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_STDDEF_H
#define INCLUDE_PROTURA_STDDEF_H

#define NULL ((void *)0)

#define offsetof(s, m) ((size_t)&(((s *)0)->m))

#define alignof(t) __alignof__(t)

#define container_of(ptr, type, member) ({ \
        const typeof(((type *)0)->member) *__mptr = (ptr); \
        (type *)((char *)__mptr - offsetof(type, member)); })

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

#define TP2(x, y) x ## y
#define TP(x, y) TP2(x, y)

/* Macro black-magic
 *
 * The end result of this macro is that it can be used in the following fashion:
 *
 * using_cond(cond, cmd1, cmd2) {
 *    ... Code ...
 * } else {
 *    ... Error handling ...
 * }
 *
 * Where the condition 'cond' is check to be 'true' (non-zero), and if that is
 * the case 'cmd1' will be executed. Then, at the end of the using_cond()
 * block, 'cmd2' will be executed. Both commands will only execute once, and
 * neither command will be executed in the event that 'cond' is false.
 * More-over, the 'else' block will be run if 'cond' fails, and the using_cond
 * block won't be executed.
 *
 * This is mostly useful for locking situations, where you can take a lock, run
 * the code inside of the using_cond block, and then release the lock. The nice
 * part of this is that for simple blocks like this you can't forget to release
 * the lock because the release is part of the using_cond statement.
 *
 * Another nicety, 'break'  and 'continue' will both exit the using_cond *and*
 * still release the locks. 'return' will *not* release the lock though, so
 * return should basically never be used inside a using_cond.
 *
 *
 * The actual macro itself isn't *too* complicated, just follow the 'goto's
 * through the code, just fairly messy. The key to reading this code is that
 * most of it will be removed by the compiler, and a lot of the blocks are
 * simply used to allow us to jump to another section of code. The reason it is
 * so weird looking is so it works as a prefix statement. IE. You can use it
 * with or without a block, and it doesn't require any 'using_end' macro after
 * its usage.
 */
#define using_cond(cond, cmd1, cmd2)                               \
    if (0)                                                         \
        TP(__using_finished, __LINE__): ;                          \
    else                                                           \
        for (int __using_cond = 0;;)                               \
            if (1)                                                 \
                goto TP(__using_body_init, __LINE__);              \
            else                                                   \
                while (1)                                          \
                    if (1) {                                       \
                        if (__using_cond)                          \
                            cmd2;                                  \
                        goto TP(__using_finished, __LINE__);       \
                        TP(__using_body_init, __LINE__):           \
                        __using_cond = (cond);                     \
                        if (__using_cond)                          \
                            cmd1;                                  \
                        goto TP(__using_body, __LINE__);           \
                    } else                                         \
                        TP(__using_body, __LINE__):                \
                        if (__using_cond)

/* The 'nocheck' version doesn't take a condition, just two commands to run.
 *
 * As a note, gcc is smart enough to turn this usage into two direct calls to
 * 'cmd1' and 'cmd2' at the beginning and end of the block, without any extra
 * code or variables. */
#define using_nocheck(cmd1, cmd2) using_cond(1, cmd1, cmd2)

/* The normal 'using' one uses the result of 'cmd1' to decide whether or not to
 * run the code. Used in the event the command you would use as 'cmd1' returns
 * an error code you want to check before you keep going. */
#define using(cmd1, cmd2) using_cond(cmd1, do { ; } while (0), cmd2)

/* This is defined by the Makefile, but it's useful to have it defined anyway,
 * so syntax checkers don't barf saying 'PROTURA_BITS' doesn't exist */
#ifndef PROTURA_BITS
# define PROTURA_BITS 32
#endif

#endif
