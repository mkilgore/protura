
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv, char **envp)
{
    int i;


    printf("argc: %d\n", argc);

    for (i = 0; i < argc; i++) {
        printf("str: %s\n", argv[i]);
    }

    argv[0][1] = 'k';

    printf("Modified arg: %s\n", argv[0]);

    printf("envp: %p\n", envp);

    while (*envp) {
        printf("ENV: %s\n", *envp);
        envp++;
    }

    printf("environ: %p\n", environ);
    printf("PATH: %s\n", getenv("PATH"));

    return 0;
}

