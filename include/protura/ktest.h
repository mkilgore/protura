#ifndef INCLUDE_PROTURA_KTEST_H
#define INCLUDE_PROTURA_KTEST_H

/*
 * ktest - a unit testing framework for the kernel. See `docs/ktest.md` for documentation.
 */

#include <protura/types.h>
#include <protura/stddef.h>
#include <protura/bits.h>

struct ktest;

enum ktest_arg_type {
    KTEST_ARG_INT,
    KTEST_ARG_UINT,
    KTEST_ARG_STR,
    KTEST_ARG_USER_BUF,
};

struct ktest_arg {
    const char *name;
    enum ktest_arg_type type;
    union {
        int intarg;
        unsigned int uintarg;
        const char *strarg;
        struct user_buffer user_buf;
    };
};

/*
 * This is pretty nasty, but the DEFER()'s are necessary to make `COUNT_ARGS`
 * work correctly down below.
 *
 * If the expansion is not deferred, the expansion happens before `COUNT_ARGS`
 * is evaluated and the commas that are part of the initializers end-up being
 * treated as separate arguments to `COUNT_ARGS`. By deferring the expansion,
 * `COUNT_ARGS` sees the `__KT_INT(#i, i)` which it will correctly count as one
 * argument.
 */
#define KT_INT(i) DEFER(__KT_INT)(#i, (i))
#define __KT_INT(si, i) \
    { \
        .name = (si), \
        .type = (KTEST_ARG_INT), \
        .intarg = (i), \
    }

#define KT_UINT(i) DEFER(__KT_UINT)(#i, (i))
#define __KT_UINT(si, i) \
    { \
        .name = (si), \
        .type = (KTEST_ARG_UINT), \
        .uintarg = (i), \
    }

#define KT_STR(s) DEFER(__KT_STR)(#s, (s))
#define __KT_STR(ss, s) \
    { \
        .name = (ss), \
        .type = (KTEST_ARG_STR), \
        .strarg = (s), \
    }

#define KT_USER_BUF(s) DEFER(__KT_USER_BUF)("user " #s, (s))
#define __KT_USER_BUF(ss, s) \
    { \
        .name = (ss), \
        .type = KTEST_ARG_USER_BUF, \
        .user_buf = make_user_buffer(s), \
    }

#define KT_KERNEL_BUF(s) DEFER(__KT_KERNEL_BUF)("krnl " #s, (s))
#define __KT_KERNEL_BUF(ss, s) \
    { \
        .name = (ss), \
        .type = KTEST_ARG_USER_BUF, \
        .user_buf = make_kernel_buffer(s), \
    }

struct ktest_unit {
    void (*test) (struct ktest *);
    const char *name;
    struct ktest_arg args[6];
    int arg_count;
};

const struct ktest_arg *ktest_get_arg(struct ktest *, int index);

#define KT_ARG_TYPE(typ) \
    ({ \
        typeof(typ) __unused ____tmp_generic; \
        _Generic(____tmp_generic, \
            int: KTEST_ARG_INT, \
            unsigned int: KTEST_ARG_UINT, \
            const char *: KTEST_ARG_STR, \
            struct user_buffer: KTEST_ARG_USER_BUF); \
    })

#define KT_ARG_TYPE_STR(typ) \
    ({ \
        const char *__ret = ""; \
        if ((typ) == KTEST_ARG_INT) \
            __ret = "int"; \
        else if ((typ) == KTEST_ARG_UINT) \
            __ret = "unsigned int"; \
        else if ((typ) == KTEST_ARG_STR) \
            __ret = "const char *"; \
        else if ((typ) == KTEST_ARG_USER_BUF) \
            __ret = "struct user_buffer"; \
        __ret; \
    })

/*
 * This takes the `struct ktest_unit`, the argument number, and
 * type, and resolves the arugment for you.
 *
 * We can't directly pass the type name to _Generic() because it's stupid, so
 * instead we just declare a temporary with the passed-in type and use that for
 * _Generic().
 */
#define KT_ARG(kt, i, typ) \
    ({ \
        typeof(typ) __unused ____tmp_generic; \
        \
        const struct ktest_arg *__arg = ktest_get_arg((kt), (i)); \
        \
        if (__arg->type != KT_ARG_TYPE(typ)) \
            ktest_assert_fail((kt), "Argument type check failure, '%s' != '%s'!\n", \
                    KT_ARG_TYPE_STR(__arg->type),KT_ARG_TYPE_STR(KT_ARG_TYPE(typ))); \
        \
        _Generic(____tmp_generic, \
            int: __arg->intarg, \
            unsigned int: __arg->uintarg, \
            const char *: __arg->strarg, \
            struct user_buffer: __arg->user_buf); \
    })

#define KTEST_UNIT(nm, t, ...) \
    { \
        .test = (t), \
        .name = (nm), \
        .args = { __VA_ARGS__ }, \
        .arg_count = COUNT_ARGS(__VA_ARGS__), \
    }

struct ktest_module {
    const char *name;

    void *priv;

    int (*setup_module) (struct ktest_module *);
    int (*teardown_module) (struct ktest_module *);

    const struct ktest_unit *tests;
    size_t test_count;
};

#define KTEST_MODULE_INIT(nam, tsts) \
    { \
        .name = (nam), \
        .tests = (tsts), \
        .test_count = ARRAY_SIZE((tsts)), \
    }

#define KTEST_MODULE_DEFINE(nam, tst, setup, teardown) \
    static const __ktest struct ktest_module TP(ktest_module, __COUNTER__) = \
        { \
            .name = (nam), \
            .tests = (tst), \
            .setup_module = (setup), \
            .teardown_module = (teardown), \
            .test_count = ARRAY_SIZE((tst)), \
        }

#define __ktest __used __section(".ktest")

enum ktest_value_type {
    KTEST_VALUE_INT64,
    KTEST_VALUE_UINT64,
    KTEST_VALUE_PTR,
    KTEST_VALUE_STR,
    KTEST_VALUE_MEM,
};

struct ktest_value {
    enum ktest_value_type type;
    const char *value_string;
    union {
        int64_t int64;
        uint64_t uint64;
        struct {
            const char *ptr;
            size_t len;
        };
    };
};

static inline struct ktest_value __ktest_make_int64(const char *str, int64_t int64)
{
    return (struct ktest_value) { .type = KTEST_VALUE_INT64, .value_string = str, .int64 = int64 };
}

static inline struct ktest_value __ktest_make_uint64(const char *str, uint64_t uint64)
{
    return (struct ktest_value) { .type = KTEST_VALUE_UINT64, .value_string = str, .uint64 = uint64 };
}

static inline struct ktest_value __ktest_make_ptr(const char *str, const void *ptr)
{
    return (struct ktest_value) { .type = KTEST_VALUE_PTR, .value_string = str, .ptr = ptr };
}

static inline struct ktest_value __ktest_make_str(const char *str, const void *ptr)
{
    return (struct ktest_value) { .type = KTEST_VALUE_STR, .value_string = str, .ptr = ptr };
}

static inline struct ktest_value __ktest_make_mem(const char *str, const void *ptr, size_t len)
{
    return (struct ktest_value) { .type = KTEST_VALUE_MEM, .value_string = str, .len = len, .ptr = ptr };
}

#define __ktest_make_value(vs, v) \
    _Generic((v), \
        int64_t: __ktest_make_int64, \
        uint64_t: __ktest_make_uint64, \
        default: _Generic(((v) - (v)), \
            ptrdiff_t: __ktest_make_ptr, \
            default: __ktest_make_int64 \
        ) \
    ) (vs, v)

#define ktest_assert_equal(kt, expect, act) \
    do { \
        struct ktest_value v1 = __ktest_make_value(#expect, expect); \
        struct ktest_value v2 = __ktest_make_value(#act, act); \
        ktest_assert_equal_value_func((kt), &v1, &v2, __func__, __LINE__); \
    } while (0)

#define ktest_assert_equal_str(kt, expect, act) \
    do { \
        struct ktest_value v1 = __ktest_make_str(#expect, expect); \
        struct ktest_value v2 = __ktest_make_str(#act, act); \
        ktest_assert_equal_value_func((kt), &v1, &v2, __func__, __LINE__); \
    } while (0)

#define ktest_assert_notequal(kt, expect, act) \
    do { \
        struct ktest_value v1 = __ktest_make_value(#expect, expect); \
        struct ktest_value v2 = __ktest_make_value(#act, act); \
        ktest_assert_notequal_value_func((kt), &v1, &v2, __func__, __LINE__); \
    } while (0)

#define ktest_assert_equal_mem(kt, expect, act, len) \
    do { \
        struct ktest_value v1 = __ktest_make_mem(#expect, (expect), (len)); \
        struct ktest_value v2 = __ktest_make_mem(#act, (act), (len)); \
        ktest_assert_equal_value_func((kt), &v1, &v2, __func__, __LINE__); \
    } while (0)


void ktest_assert_equal_value_func(struct ktest *, struct ktest_value *expected, struct ktest_value *actual, const char *func, int lineno);
void ktest_assert_notequal_value_func(struct ktest *, struct ktest_value *expected, struct ktest_value *actual, const char *func, int lineno);

void ktest_assert_failv(struct ktest *, const char *fmt, va_list);
void ktest_assert_fail(struct ktest *, const char *fmt, ...) __printf(2, 3);

void ktest_init(void);

#endif
