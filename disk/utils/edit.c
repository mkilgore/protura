
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int main(int argc, char **argv, char **envp)
{
    const char *filename;
    FILE *file;
    int c;

    if (argc != 2) {
        fprintf(stderr, "%s: Please supply the name of the file to edit\n", argv[0]);
        return 0;
    }

    filename = argv[1];

    file = fopen(filename, "w");

    while ((c = getchar()) != EOF && c != 4) {
        putchar(c);
        fputc(c, file);
    }

    fclose(file);

    return 0;
}

