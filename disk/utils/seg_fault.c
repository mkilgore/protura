

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

char *no_terminating_char = "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            "THIS IS A REALLY LONG STRONG THAT IS LONGER THEN PATH_MAX"
                            ;


int main(int argc, char **argv)
{
    volatile char *ptr = 0;
    int ret;

    ret = open(no_terminating_char, 0, 0);

    printf("ret = %d\n", ret);

    ret = read(0, NULL, 40);

    printf("ret = %d\n", ret);

    ret = write(1, NULL, 410000);

    printf("ret = %d\n", ret);

    *ptr = 2;

    return 0;
}

