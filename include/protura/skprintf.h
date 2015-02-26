#ifndef INCLUDE_PROTURA_SKPRINTF
#define INCLUDE_PROTURA_SKPRINTF

#include <stdarg.h>

void skprintfv(char *buf, const char *s, va_list);
void skprintf(char *buf, const char *s, ...);

#endif
