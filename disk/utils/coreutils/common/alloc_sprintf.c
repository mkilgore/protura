
#include "common.h"

#include <stdlib.h>
#include <stdarg.h>

int a_sprintfv(char **buf, const char *format, va_list lst)
{
    va_list cpy;
    va_copy(cpy, lst);

    size_t size = vsnprintf(NULL, 0, format, cpy) + 1;

    *buf = realloc(*buf, size);

    if (!*buf)
        return -1;

    (*buf)[0] = '\0';

    return vsnprintf(*buf, size, format, lst);
}

int a_sprintf(char **buf, const char *format, ...)
{
    size_t ret;
    va_list lst;

    va_start(lst, format);
    ret = a_sprintfv(buf, format, lst);
    va_end(lst);

    return ret;
}

