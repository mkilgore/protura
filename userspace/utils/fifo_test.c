
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>

#define WRITER_COUNT 5

const char *fifo;

void reader_prog(void)
{
    char buf[100];
    size_t len;
    int fd = open(fifo, O_RDONLY);

    printf("Fifo open for reading!\n");

    while ((len = read(fd, buf, sizeof(buf))) != 0)
        write(STDOUT_FILENO, buf, len);

    printf("Read side returned EOF\n");
    close(fd);
    printf("Read side closed...\n");

    return ;
}

int writer_id;

void writer_prog(void)
{
    char buf[100];
    size_t len;
    int i;
    int fd;

    srand(time(NULL) ^ (writer_id << 2));

    len = snprintf(buf, sizeof(buf), "Writer %d msg\n", writer_id);

    printf("Writer %d opening fifo.\n", writer_id);
    fd = open(fifo, O_WRONLY);
    printf("Writer %d open!\n", writer_id);

    for (i = 0; i < 20; i++) {
        //poll(NULL, 0, rand() % 2000);
        write(fd, buf, len);
    }

    printf("Writer %d closing fifo.\n", writer_id);
    close(fd);

    return ;
}

void fork_prog(void (*prog) (void))
{
    switch (fork()) {
    case 0:
        (prog) ();
        exit(0);

    case -1:
        perror("fork()");
        exit(1);

    default:
        break;
    }
}

int main(int argc, char **argv)
{
    int i;

    if (argc != 2) {
        printf("%s: Please provide fifo name\n", argv[0]);
        return 0;
    }

    fifo = argv[1];

    fork_prog(reader_prog);

    for (i = 0; i < WRITER_COUNT; i++) {
        writer_id = i;
        fork_prog(writer_prog);
    }

    wait(NULL);

    for (i = 0; i < WRITER_COUNT; i++)
        wait(NULL);

    return 0;
}
