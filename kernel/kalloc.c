/* Physical memory allocator, for user processes, kernel stacks,
 * page-table pages, and pipe buffers. Allocates 4K pages.
 */

#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

extern char end[]; /* First address after kernel, set by linker */

struct page {
  struct page *next;
};

struct kmem {
  struct spinlock lock;
  struct page *freelist;
};

static struct kmem kmem;

void kalloc_init()
{
  struct page *p = (struct page *)PGROUNDUP((unsigned long)end);

  /* Add all available pages to the free list */
  for (; p + PGSIZE <= (struct page *)PHYSTOP; p += PGSIZE) {
    p->next = kmem.freelist;
    kmem.freelist = p;
  }

  initlock(&kmem.lock);
}

/* Give page back to the free list */
void kfree(void *pa)
{
  struct page *p = pa;

  if (((unsigned long)pa % PGSIZE) != 0 || (char*)pa < end || (unsigned long)pa >= PHYSTOP)
    panic("kfree: page not aligned or out of bounds");

  acquire(&kmem.lock);
  p->next = kmem.freelist;
  kmem.freelist = p;
  release(&kmem.lock);
}

/* Returns one 4K page from the free list */
void *kalloc()
{
  struct page *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if (r)
    kmem.freelist = kmem.freelist->next;
  release(&kmem.lock);

  return (void*)r;
}
