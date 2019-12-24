
#include "common.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

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

int a_sprintfv_append(char **buf, const char *format, va_list lst)
{
    size_t buf_len = 0;
    va_list cpy;
    va_copy(cpy, lst);

    if (*buf)
        buf_len = strlen(*buf);

    size_t size = vsnprintf(NULL, 0, format, cpy) + buf_len + 1;

    *buf = realloc(*buf, size);

    if (!*buf)
        return -1;

    if (!buf_len)
        (*buf)[0] = '\0';

    return vsnprintf(*buf + buf_len, size - buf_len, format, lst);
}

int a_sprintf_append(char **buf, const char *format, ...)
{
    size_t ret;
    va_list lst;

    va_start(lst, format);
    ret = a_sprintfv_append(buf, format, lst);
    va_end(lst);

    return ret;
}

