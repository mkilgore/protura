
#include <stdio.h>
#include <unistd.h>

#define begin_str "Echo!\n" "Type '~' to exit\n"

int main(int argc, char **argv)
{
    write(1, begin_str, sizeof(begin_str) - 1);

    char c = '\0';
    while (c != '~') {
        c = getchar();
        /* read(0, &c, 1); */
        write(1, &c, 1);
    }

    return 0;
}
