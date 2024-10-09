/* Manages page tables and address spaces
 */

#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

unsigned long * kernel_pagetable; /* Pointer to the kernel's root page-table page*/

extern char etext[];  /* kernel.ld sets this to end of kernel code. */
extern char trampoline[];

/* Make a direct-map page table for the kernel. */
static unsigned long * kvm_make()
{
  unsigned long * kpgtbl = (unsigned long *) kalloc();

  kvm_map(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  kvm_map(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  kvm_map(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  kvm_map(kpgtbl, KERNBASE, KERNBASE, (unsigned long)etext-KERNBASE, PTE_R | PTE_X);
  kvm_map(kpgtbl, (unsigned long)etext, (unsigned long)etext, PHYSTOP-(unsigned long)etext, PTE_R | PTE_W);
  kvm_map(kpgtbl, TRAMPOLINE, (unsigned long)trampoline, PGSIZE, PTE_R | PTE_X);

  /* Allocate and map a kernel stack for each process. */
  proc_mapstacks(kpgtbl);
  
  return kpgtbl;
}

/* Initialize the one kernel pagetable */
void kvm_init()
{
  kernel_pagetable = kvm_make();
}

/* Switch HW page table register to the kernel's page table,
 * effectively enabling paging.
 */
void kvm_init_hart()
{
  /* Wait for any previous writes to the page table memory to finish. */
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  /* Flush stale entries from the TLB. */
  sfence_vma();
}

/* Find the PTE given a virtual address.  If alloc != 0, create any required
 * page-table pages.
 *
 * xv7 uses a 3-level page table scheme, where a page-table page contains
 * 512 64-bit PTEs.
 * A 64-bit virtual address is split into five fields:
 *   39..63 -- Unused.
 *   30..38 -- 9 bits of level-2 index.
 *   21..29 -- 9 bits of level-1 index.
 *   12..20 -- 9 bits of level-0 index.
 *   0..11 -- 12 bits of byte offset within the page.
 */
static unsigned long * walk(unsigned long * pagetable, unsigned long va, int alloc)
{
  unsigned long *pte;

  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--) {
    pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V) {
      pagetable = (unsigned long *)PTE2PA(*pte);
    } else {
      if (!alloc || !(pagetable = kalloc()))
        return 0;

      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }

  return &pagetable[PX(0, va)];
}

/* Look up a virtual address, return the physical address,
 * or 0 if not mapped. Can only be used to look up user pages.
 */
unsigned long walkaddr(unsigned long * pagetable, unsigned long va)
{
  unsigned long *pte;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0 || !(*pte & PTE_V) || !(*pte & PTE_U))
    return 0;

  return PTE2PA(*pte);
}

/* Add a mapping to the kernel page table. */
void kvm_map(unsigned long * kpgtbl, unsigned long va, unsigned long pa, unsigned long sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvm_map");
}

/* Add PTEs to the pagetable for va that refers to pa. */
int mappages(unsigned long * pagetable, unsigned long va, unsigned long size, unsigned long pa, int perm)
{
  unsigned long a, last, *pte;

  if (size == 0)
    panic("mappages: size");

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for (;;) {
    if (!(pte = walk(pagetable, a, 1)))
      return -1;

    if (*pte & PTE_V)
      panic("mappages: remap");

    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;

    a += PGSIZE, pa += PGSIZE;
  }

  return 0;
}

/* Remove npages of mappings starting from va. */
void uvm_unmap(unsigned long * pagetable, unsigned long va, unsigned long npages, int free)
{
  unsigned long a, *pte;

  if ((va % PGSIZE) != 0)
    panic("uvm_unmap: not aligned");

  for (a = va; a < va + npages*PGSIZE; a += PGSIZE) {
    if ((pte = walk(pagetable, a, 0)) == 0)
      panic("uvm_unmap: walk");

    if ((*pte & PTE_V) == 0)
      panic("uvm_unmap: not mapped");

    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvm_unmap: not a leaf");

    if (free)
      kfree((void*)PTE2PA(*pte));

    *pte = 0;
  }
}

/* Load the user initcode into address 0 of pagetable, for the very first process. */
void uvm_first(unsigned long * pagetable, unsigned char *src, unsigned int sz)
{
  char *mem = kalloc();

  if (sz >= PGSIZE)
    panic("uvm_first: more than a page");

  mappages(pagetable, 0, PGSIZE, (unsigned long)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

/* Allocate PTEs and physical memory to grow process from oldsz to newsz,
 * which need not be page aligned.
 */
unsigned long uvm_alloc(unsigned long * pagetable, unsigned long oldsz, unsigned long newsz, int xperm)
{
  unsigned long a;
  char *mem;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE) {
    mem = kalloc();
    if (!mem) {
      uvm_dealloc(pagetable, a, oldsz);
      return 0;
    }

    if (mappages(pagetable, a, PGSIZE, (unsigned long)mem, PTE_R|PTE_U|xperm)) {
      kfree(mem);
      uvm_dealloc(pagetable, a, oldsz);
      return 0;
    }
  }

  return newsz;
}

/* Deallocate user pages to bring the process size from oldsz to newsz.
 * Returns the new process size.
 */
unsigned long uvm_dealloc(unsigned long * pagetable, unsigned long oldsz, unsigned long newsz)
{
  int npages;

  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz)) {
    npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvm_unmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

/* Recursively free page-table pages. */
static void freewalk(unsigned long * pagetable)
{
  unsigned long pte;

  /* There are 2^9 = 512 PTEs per page table. */
  for (int i = 0; i < 512; i++) {
    pte = pagetable[i];
    if ((pte & PTE_V) && !(pte & (PTE_R|PTE_W|PTE_X))) {
      /* This PTE points to a lower-level page table. */
      freewalk((unsigned long *)PTE2PA(pte));
      pagetable[i] = 0;
    } else if (pte & PTE_V) {
      panic("freewalk: leaf");
    }
  }

  kfree((void*)pagetable);
}

/* Free user memory pages, then free page-table pages. */
void uvm_free(unsigned long * pagetable, unsigned long sz)
{
  if (sz > 0)
    uvm_unmap(pagetable, 0, PGROUNDUP(sz)/PGSIZE, 1);

  freewalk(pagetable);
}

/* Given a parent process's page table, copy its memory into a child's page table. */
int uvm_copy(unsigned long * old, unsigned long * new, unsigned long sz)
{
  unsigned long *pte, pa, i;
  unsigned int flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE) {
    if (!(pte = walk(old, i, 0)))
      panic("uvm_copy: pte should exist");

    if (!(*pte & PTE_V))
      panic("uvm_copy: page not present");

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
      goto err;

    memmove(mem, (char*)pa, PGSIZE);

    if (mappages(new, i, PGSIZE, (unsigned long)mem, flags)) {
      kfree(mem);
      goto err;
    }
  }

  return 0;

 err:
  uvm_unmap(new, 0, i / PGSIZE, 1);
  return -1;
}

/* Mark a PTE invalid for user access. */
void uvm_clear(unsigned long * pagetable, unsigned long va)
{
  unsigned long *pte = walk(pagetable, va, 0);

  if (!pte)
    panic("uvm_clear");

  *pte &= ~PTE_U;
}

/* Copy from kernel to user. */
int copy_to_user(unsigned long * pagetable, unsigned long dstva, char *src, unsigned long len)
{
  unsigned long n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;

    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;

    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n, src += n, dstva = va0 + PGSIZE;
  }

  return 0;
}

/* Copy len bytes to dst from virtual address srcva in a given page table. */
int copy_from_user(unsigned long * pagetable, char *dst, unsigned long srcva, unsigned long len)
{
  unsigned long n, va0, pa0;

  while (len > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;

    n = PGSIZE - (srcva - va0);
    if (n > len)
      n = len;

    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n, dst += n, srcva = va0 + PGSIZE;
  }

  return 0;
}

/* Copy a null-terminated string from user to kernel. */
int copyin_str(unsigned long * pagetable, char *dst, unsigned long srcva, unsigned long max)
{
  unsigned long n, va0, pa0;
  int got_null = 0;
  char *p;

  while (got_null == 0 && max > 0) {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;

    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    p = (char *) (pa0 + (srcva - va0));
    while (n > 0) {
      if (*p == '\0') {
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n, --max, p++, dst++;
    }

    srcva = va0 + PGSIZE;
  }

  if (got_null)
    return 0;

  return -1;
}
