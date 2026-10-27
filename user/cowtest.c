#include "kernel/types.h"
#include "user/user.h"

#define PGSIZE 4096
#define NPAGES 20000 // ~78MB: fork() would run out of memory without COW

static char big[NPAGES * PGSIZE];

static void
check(int ok, char *msg)
{
  if (!ok) {
    printf("FAIL: %s\n", msg);
    exit(1);
  }
}

// a big address space must fork without copying it
static void
largefork(void)
{
  int pid, st = -1;

  pid = fork();
  check(pid >= 0, "largefork: fork failed (is COW working?)");
  if (pid == 0) {
    big[NPAGES * PGSIZE - 1] = 1; // touch a single page
    exit(0);
  }
  check(wait(&st) == pid, "largefork: wait failed");
  check(st == 0, "largefork: child failed");
  printf("largefork: ok\n");
}

// child writes; parent must not see it
static void
childwrites(void)
{
  int i, pid, st = -1;

  for (i = 0; i < 16; i++)
    big[i * PGSIZE] = i + 1;

  pid = fork();
  check(pid >= 0, "childwrites: fork failed");
  if (pid == 0) {
    for (i = 0; i < 16; i++) {
      check(big[i * PGSIZE] == i + 1, "childwrites: child lost parent data");
      big[i * PGSIZE] = 100 + i; // triggers COW
    }
    exit(0);
  }
  check(wait(&st) == pid, "childwrites: wait failed");
  check(st == 0, "childwrites: child failed");
  for (i = 0; i < 16; i++)
    check(big[i * PGSIZE] == i + 1, "childwrites: parent data overwritten");
  printf("childwrites: ok\n");
}

// parent writes first; child must keep the old contents
static void
parentwrites(void)
{
  int pid, st = -1, fd[2];
  char c;

  big[0] = 7;
  check(pipe(fd) == 0, "parentwrites: pipe");
  pid = fork();
  check(pid >= 0, "parentwrites: fork failed");
  if (pid == 0) {
    read(fd[0], &c, 1); // wait until the parent has written
    check(big[0] == 7, "parentwrites: child sees parent's write");
    exit(0);
  }
  big[0] = 9;
  write(fd[1], "x", 1);
  check(wait(&st) == pid, "parentwrites: wait failed");
  check(st == 0, "parentwrites: child failed");
  check(big[0] == 9, "parentwrites: parent lost its write");
  printf("parentwrites: ok\n");
}

// the kernel writes into a COW page through read(): copyout must break sharing
static void
copyouttest(void)
{
  int i, pid, st = -1, fd[2];
  char *p = big + 5 * PGSIZE;
  char msg[100];

  memset(p, 'a', PGSIZE);
  for (i = 0; i < 100; i++)
    msg[i] = 'z';
  check(pipe(fd) == 0, "copyouttest: pipe");
  pid = fork();
  check(pid >= 0, "copyouttest: fork failed");
  if (pid == 0) {
    check(read(fd[0], p + 10, 100) == 100, "copyouttest: read");
    for (i = 10; i < 110; i++)
      check(p[i] == 'z', "copyouttest: child data wrong");
    exit(0);
  }
  write(fd[1], msg, 100);
  check(wait(&st) == pid, "copyyouttest: wait failed");
  check(st == 0, "copyouttest: child failed");
  for (i = 0; i < PGSIZE; i++)
    check(p[i] == 'a', "copyouttest: parent page changed");
  printf("copyouttest: ok\n");
}

int
main(void)
{
  largefork();
  childwrites();
  parentwrites();
  copyouttest();
  printf("cowtest: all ok\n");
  exit(0);
}