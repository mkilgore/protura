
#include <unistd.h>
#include <ctype.h>

int stringcasecmp(const char *s1, const char *s2)
{
    for (; *s1 && *s2; s1++, s2++)
        if (toupper(*s1) != toupper(*s2))
            return 1;

    if (*s1 || *s2)
        return 1;

    return 0;
}

int stringncasecmp(const char *s1, const char *s2, size_t n)
{
    size_t i;
    for (i = 0; *s1 && *s2 && i < n; s1++, s2++, i++)
        if (toupper(*s1) != toupper(*s2))
            return 1;

    if (*s1 || *s2)
        return 1;

    return 0;
}


