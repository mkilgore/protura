
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int count = 0;
    int c;

    while ((c = getchar()) != EOF) {
        putchar(c);

        if (c == '\n') {
            count++;
            if (count == 5)
                break;
        }
    }

    return 0;
}

