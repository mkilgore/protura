#ifndef INCLUDE_PROTURA_KTEST_H
#define INCLUDE_PROTURA_KTEST_H

#include <protura/types.h>

struct ktest;

struct ktest_unit {
    int (*test) (struct ktest *);
    const char *name;
};

#define KTEST_UNIT_INIT(nm, t) \
    { \
        .test = (t), \
        .name = (nm), \
    }

struct ktest_module {
    const char *name;
    const struct ktest_unit *tests;
    size_t test_count;
};

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

static inline struct ktest_value __ktest_make_mem(const char *str, size_t len, const void *ptr)
{
    return (struct ktest_value) { .type = KTEST_VALUE_MEM, .value_string = str, .len = len, .ptr = ptr };
}

#define __ktest_make_value(vs, v) \
    _Generic((v), \
        int64_t: __ktest_make_int64, \
        uint64_t: __ktest_make_uint64, \
        char *: __ktest_make_str, \
        const char *: __ktest_make_str, \
        default: _Generic((v - v), \
            ptrdiff_t: __ktest_make_ptr, \
            default: __ktest_make_int64 \
        ) \
    ) (vs, v)

#define ktest_assert_equal(kt, expect, act) \
    ({ \
        struct ktest_value v1 = __ktest_make_value(#expect, expect); \
        struct ktest_value v2 = __ktest_make_value(#act, act); \
        ktest_assert_equal_value_func((kt), &v1, &v2, __func__); \
    })

#define ktest_assert_notequal(kt, expect, act) \
    ({ \
        struct ktest_value v1 = __ktest_make_value(#expect, expect); \
        struct ktest_value v2 = __ktest_make_value(#act, act); \
        ktest_assert_notequal_value_func((kt), &v1, &v2, __func__); \
    })

#define ktest_assert_mem(kt, expect, act, len) \
    ({ \
        struct ktest_value v1 = __ktest_make_mem(#expect, (expect), (len)); \
        struct ktest_value v2 = __ktest_make_mem(#act, (act), (len)); \
        ktest_assert_notequal_value_func((kt), &v1, &v2, __func__); \
    })

int ktest_assert_equal_value_func(struct ktest *, struct ktest_value *expected, struct ktest_value *actual, const char *func);
int ktest_assert_notequal_value_func(struct ktest *, struct ktest_value *expected, struct ktest_value *actual, const char *func);

void ktest_init(void);

#endif
