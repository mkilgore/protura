
#include "common.h"

#include <string.h>
#include <stdlib.h>

char *strdupx(const char *str)
{
    if (!str)
        return NULL;

    return strdup(str);
}
