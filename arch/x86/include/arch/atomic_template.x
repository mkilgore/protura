
#define MAKE_NAME(x, y, z) TP(x, TP(y, z))

#define TYPE MAKE_NAME(int, BITS, _t)
#define ATOMIC_TYPE MAKE_NAME(atomic, BITS, _t)

#define ATOMIC_FUNC(name) TP(MAKE_NAME(atomic, BITS, _), name)

typedef struct {
    TYPE counter;
} ATOMIC_TYPE;

static __always_inline TYPE ATOMIC_FUNC(get)(const ATOMIC_TYPE *v)
{
    return (*(volatile TYPE *)&(v)->counter);
}

static __always_inline void ATOMIC_FUNC(set)(ATOMIC_TYPE *v, TYPE i)
{
    v->counter = i;
}

static __always_inline void ATOMIC_FUNC(inc)(ATOMIC_TYPE *v)
{
    asm volatile(LOCK_PREFIX "inc %0"
            : "+m" (v->counter));
}

static __always_inline void ATOMIC_FUNC(dec)(ATOMIC_TYPE *v)
{
    asm volatile(LOCK_PREFIX "dec %0"
            : "+m" (v->counter));
}

static __always_inline void ATOMIC_FUNC(add)(ATOMIC_TYPE *v, TYPE i)
{
    asm volatile(LOCK_PREFIX "add %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

static __always_inline void ATOMIC_FUNC(sub)(ATOMIC_TYPE *v, TYPE i)
{
    asm volatile(LOCK_PREFIX "sub %1, %0"
            : "+m" (v->counter)
            : "ir" (i));
}

#undef ATOMIC_FUNC
#undef ATOMIC_TYPE
#undef TYPE
#undef MAKE_NAME

