
#include <stdio.h>
#include <time.h>

int main()
{
    time_t t = time(NULL);
    struct tm tm;
    char buf[128];

    tm = *gmtime(&t);

    strftime(buf, sizeof(buf), "%D %T", &tm);

    printf("%s\n", buf);

    return 0;
}
