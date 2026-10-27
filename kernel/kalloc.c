// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

static int pgref[(PHYSTOP - KERNBASE) / PGSIZE];
#define PGIDX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    pgref[PGIDX(p)] = 1; // kfree() drops this to 0 and puts the page on the free list
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Drop one reference; only the last one really frees the pages.
  acquire(&kmem.lock);
  if (pgref[PGIDX(pa)] < 1)
    panic("kfree: refcount");
  if (--pgref[PGIDX(pa)] > 0) {
    release(&kmem.lock);
    return;
  }
  release(&kmem.lock);

  // Nobody maps the page any more, so it is safe to scribble on it
  // outside the lock.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r) {
    kmem.freelist = r->next;
    pgref[PGIDX(r)] = 1;
  }
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// Add a reference to the page at pa.
void
kref(void *pa)
{
  acquire(&kmem.lock);
  if (pgref[PGIDX(pa)] < 1)
    panic("kref");
  pgref[PGIDX(pa)]++;
  release(&kmem.lock);
}

// Number of references to the page at pa.
int
krefcount(void *pa)
{
  int n;

  acquire(&kmem.lock);
  n = pgref[PGIDX(pa)];
  release(&kmem.lock);
  return n;
}
