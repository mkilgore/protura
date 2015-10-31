
#include <protura/types.h>
#include <protura/string.h>

void memcpy(void *restrict dest, const void *restrict src, size_t len)
{
    asm ("cld; rep movsb"
        :
        : "D" (dest), "S" (src), "c" (len)
        );
}


void memmove(void *dest, const void *src, size_t len)
{
    if (dest < src) {
        asm ("cld; rep movsb"
            :
            : "D" (dest), "S" (src), "c" (len)
            );
    } else {
        asm ("std; rep movsb"
            :
            : "D" (dest + len - 1), "S" (src + len - 1), "c" (len)
            );
    }
}

void memset(void *ptr, int c, size_t len)
{
    asm ("cld; rep stosb"
        :
        : "D" (ptr), "a" (c), "c" (len)
        );
}

