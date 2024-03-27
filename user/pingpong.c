#include "user/user.h"

int
main(int argc, char *argv[])
{
    int pid, fds[2];
    char buf[1];

    if (pipe(fds) < 0)
        exit(1);

    pid = fork();
    if (pid < 0)
        exit(1);

    if (pid == 0) { // child
        read(fds[0], buf, 1);
        printf("%d: received ping (%s)\n", getpid(), buf);
        write(fds[1], buf, 1);
        exit(0);
    } else if (pid > 0) { // parent
        write(fds[1], "x", 1);
        read(fds[0], buf, 1);
        printf("%d: received pong (%s)\n", getpid(), buf);
        exit(0);
    }
}