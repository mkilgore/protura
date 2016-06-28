
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <protura/syscall.h>

#define INT_COUNT 128

#define Q(x) QQ(x)
#define QQ(x) #x

static int pipe(int *fds)
{
    int ret;

    asm volatile("int $" Q(INT_SYSCALL) "\n"
                 : "=a" (ret)
                 : "0" (SYSCALL_PIPE), "b" ((int)fds)
                 : "memory");

    return ret;
}

int main(int argc, char **argv)
{
    int k = 0;
    int i;
    char data[] = "It's Data!!!\n";
    char new_data[sizeof(data)] = { 0 };

    int fds[2];
    int ret;
    size_t len;

    int parent = 0;

    printf("Pipe\n");
    ret = pipe(fds);

    printf("Return: %s\n", strerror(ret));
    printf("fds[0]: %d, fds[1]: %d\n", fds[0], fds[1]);

    read(fds[0], NULL, 0);

    switch(fork())
    {
    case 0:
        /* Child */
        close(fds[0]);

        printf("Child write-to-pipe\n");
        printf("Write Data: %s\n", data);

        for (i = 0; i < 200; i++)
            write(fds[1], data, sizeof(data));

        write(fds[1], "NEW DATA!!!!\n", sizeof(data));

        printf("Done writing data!\n");

        close(fds[1]);

        printf("Writer FD closed\n");

        break;

    default:
        /* Parent */
        close(fds[1]);
        printf("Parent read-from-pipe\n");

        while ((len = read(fds[0], new_data, sizeof(data))) == sizeof(data)) {
            printf("Read %zu\t", len);
            k++;
        }

        printf("Read %d pieces of data until EOF\n", k);
        printf("Data: %s\n", new_data);

        close(fds[0]);

        /*
        for (i = 0; i < 200; i++) {
            read(fds[0], new_data, sizeof(data));
            printf("Read data: %s\n", new_data);
        }
        */

        parent = 1;
        break;

    case -1:
        /* Error */
        printf("Fork error\n");
        break;
    }

    if (parent)
        wait(NULL);

    return 0;
}

