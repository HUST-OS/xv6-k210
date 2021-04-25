// Physical memory or pages management

// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/pm.h"
#include "include/string.h"
#include "include/printf.h"

void freerange(void *pa_start, void *pa_end);

extern char kernel_end[]; // first address after kernel.

struct run {
  struct run *next;
};

static struct {
  struct spinlock lock;
  struct run *freelist;
  uint64 npage;
} kmem;

void
kpminit()
{
  initlock(&kmem.lock, "kmem");
  kmem.freelist = 0;
  kmem.npage = 0;
  freerange(kernel_end, (void*)PHYSTOP);
  #ifdef DEBUG
  printf("kernel_end: %p, phystop: %p\n", kernel_end, (void*)PHYSTOP);
  printf("kpminit\n");
  #endif
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    freepage(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to allocpage().  (The exception is when
// initializing the allocator; see kpminit above.)
void
freepage(void *pa)
{
  struct run *r;
  
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < kernel_end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  kmem.npage++;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
allocpage(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    kmem.npage--;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

uint64
idlepages(void)
{
  return kmem.npage;
}