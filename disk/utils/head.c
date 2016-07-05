
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int count = 0;
    char buf[10];
    size_t len;
    int c;

    while ((c = getchar()) != EOF) {
        if (c == '\n') {
            count++;
            if (count == 5)
                break;
        }

        putchar(c);
    }

    return 0;
}

