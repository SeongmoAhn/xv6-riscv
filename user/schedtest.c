#include "kernel/types.h"
#include "user/user.h"

#define NCHILD 3
#define WORK 400000000

static void
spin(void)
{
    volatile int x = 0;
    for (long i = 0; i < WORK; i++) {
        x++;
    }
}

int
main(void)
{
    int start = uptime();

    for (int n = 0; n < NCHILD; n++) {
        if (fork() == 0) {
            spin();
            printf("child %d done: %d ticks\n", n, uptime() - start);
            exit(0);
        }
    }
    for (int n = 0; n < NCHILD; n++)
        wait(0);
    exit(0);
}