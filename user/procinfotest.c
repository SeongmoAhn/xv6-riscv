#include "kernel/types.h"
#include "kernel/procinfo.h"
#include "user/user.h"

static char *states[] = { "unused", "used", "sleep", "runble", "run", "zombie" };

int
main(void)
{
    struct procinfo pi;
    int pid = getpid();

    if (procinfo(pid, &pi) < 0) {
        printf("FAIL: procinfo(self)\n");
        exit(1);
    }
    printf("pid=%d ppid=%d state=%s sz=%d name=%s\n", pi.pid, pi.ppid, states[pi.state], (int)pi.sz, pi.name);

    if (procinfo(9999, &pi) != -1) {
        printf("FAIL: bad pid\n");
        exit(1);
    }
    if (procinfo(pid, (struct procinfo *)0x10000000) != -1) {
        printf("FAIL: bad pointer\n");
        exit(1);
    }
    printf("procinfotest: OK\n");
    exit(0);
}