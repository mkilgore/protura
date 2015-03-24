#ifndef INCLUDE_WRAP_H
#define INCLUDE_WRAP_H

#define using_tp2(x, y) x ## y
#define using_tp(x, y) using_tp2(x, y)

#define using_cond(cond, cmd1, cmd2)                               \
    if (0)                                                         \
        using_tp(__using_finished, __LINE__): ;                    \
    else                                                           \
        for (int __using_cond = 0;;)                               \
            if (1)                                                 \
                goto using_tp(__using_body_init, __LINE__);        \
            else                                                   \
                while (1)                                          \
                    if (1) {                                       \
                        if (__using_cond)                          \
                            cmd2;                                  \
                        goto using_tp(__using_finished, __LINE__); \
                        using_tp(__using_body_init, __LINE__):     \
                        if ((cond)) {                              \
                            cmd1;                                  \
                            __using_cond = 1;                      \
                        }                                          \
                        goto using_tp(__using_body, __LINE__);     \
                    } else                                         \
                        using_tp(__using_body, __LINE__):          \
                        if (__using_cond)

#define using_nocheck(cmd1, cmd2) using_cond(1, cmd1, cmd2)
#define using(cmd1, cmd2) using_cond(cmd1, (void), cmd2)

#endif
