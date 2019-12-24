#ifndef COMMON_STRINGCASECMP_H
#define COMMON_STRINGCASECMP_H

#include <unistd.h>

int stringcasecmp(const char *s1, const char *s2);
int stringncasecmp(const char *s1, const char *s2, size_t n);

#endif
