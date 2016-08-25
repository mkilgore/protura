
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv, char **envp)
{
    int i;


    printf("argc: %d\n", argc);

    for (i = 0; i < argc; i++)
        printf("str: %s\n", argv[i]);

    printf("envp: %p\n", envp);

    while (*envp) {
        printf("ENV: %s\n", *envp);
        envp++;
    }

    for (i = 3; i < 32; i++)
        if (close(i) == 0)
            printf("FD %d valid\n", i);

    return 0;
}

