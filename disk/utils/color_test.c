
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "termcols.h"


int main(int argc, char **argv, char **envp)
{
    printf("Switching to RED/BLUE...\n");

    printf(TERM_COLOR_RED TERM_COLOR_BG_BLUE "Color was changed!");

    return 0;
}

