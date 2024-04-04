#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "pstat.h"
#include "rand.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret();
static void freeproc(struct proc *p);

extern char trampoline[];

struct spinlock wait_lock;

/* Allocate a page for each process's kernel stack.
 * Map it high in memory, followed by an invalid guard page.
 */
void proc_mapstacks(unsigned long * kpgtbl)
{
  unsigned long va;
  struct proc *p;
  char *pa;
  
  for (p = proc; p < &proc[NPROC]; p++) {
    pa = kalloc();
    if (!pa)
      panic("kalloc");

    va = KSTACK((int) (p - proc));
    kvm_map(kpgtbl, va, (unsigned long)pa, PGSIZE, PTE_R | PTE_W);
  }
}

/* Initialize the process table */
void proc_init()
{
  struct proc *p;
  
  initlock(&pid_lock);
  initlock(&wait_lock);

  for (p = proc; p < &proc[NPROC]; p++) {
      initlock(&p->lock);
      p->state = UNUSED;
      p->kstack = KSTACK((int) (p - proc));
  }
}

/* Must be called with interrupts disabled */
int cpuid()
{
  return r_tp();
}

/* Return this CPU's cpu struct. Interrupts must be disabled. */
struct cpu* mycpu()
{
  return &cpus[cpuid()];
}

/* Return the current struct proc * */
struct proc* myproc()
{
  return mycpu()->proc;
}

static int allocpid()
{
  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid++;
  release(&pid_lock);

  return pid;
}

/* Look in the process table for an UNUSED proc.
 * If found, initialize state required to run in the kernel,
 * and return with p->lock held.
 */
static struct proc * allocproc()
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED)
      goto found;
    else
      release(&p->lock);
  }

  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  /* Allocate a trapframe page. */
  p->trapframe = kalloc();
  if (!p->trapframe) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  /* An empty user page table. */
  p->pagetable = proc_pagetable(p);
  if (!p->pagetable) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  /* Set up new context to start executing at forkret, which returns to user space. */
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (unsigned long)forkret;
  p->context.sp = p->kstack + PGSIZE;

  p->ticks = 0;
  p->alarmhandler = 0;
  p->tickets = 1;

  return p;
}

/* Free a proc structure. */
static void freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void*)p->trapframe);

  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);

  p->trapframe = 0;
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->channel = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
}

/* Create a user page table for a given process with trampoline and trapframe pages. */
unsigned long * proc_pagetable(struct proc *p)
{
  unsigned long * pagetable = kalloc();

  if (!pagetable)
    return 0;

  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
              (unsigned long)trampoline, PTE_R | PTE_X) < 0) {
    uvm_free(pagetable, 0);
    return 0;
  }

  if (mappages(pagetable, TRAPFRAME, PGSIZE,
              (unsigned long)(p->trapframe), PTE_R | PTE_W) < 0) {
    uvm_unmap(pagetable, TRAMPOLINE, 1, 0);
    uvm_free(pagetable, 0);
    return 0;
  }

  return pagetable;
}

void proc_freepagetable(unsigned long * pagetable, unsigned long sz)
{
  uvm_unmap(pagetable, TRAMPOLINE, 1, 0);
  uvm_unmap(pagetable, TRAPFRAME, 1, 0);
  uvm_free(pagetable, sz);
}

/* a user program that calls exec("/init") */
unsigned char initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

/* Set up first user process. */
void userinit()
{
  struct proc *p = allocproc();

  initproc = p;
  
  /* Allocate one user page and copy initcode into it. */
  uvm_first(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  /* Prepare for the first "return" from kernel to user. */
  p->trapframe->epc = 0;
  p->trapframe->sp = PGSIZE;

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

/* Grow or shrink user memory by n bytes. */
int growproc(int n)
{
  struct proc *p = myproc();
  unsigned long sz;

  sz = p->sz;
  if (n > 0) {
    sz = uvm_alloc(p->pagetable, sz, sz + n, PTE_W);
    if (!sz)
      return -1;
  } else if (n < 0) {
    sz = uvm_dealloc(p->pagetable, sz, sz + n);
  }

  p->sz = sz;

  return 0;
}

/* Create a new process, copying the parent.
 * Sets up child kernel stack to return as if from fork() system call.
 */
int fork()
{
  struct proc *np = allocproc(), *p = myproc();
  int i, pid;

  if (!np)
    return -1;

  /* Copy user memory from parent to child. */
  if (uvm_copy(p->pagetable, np->pagetable, p->sz) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }

  np->sz = p->sz;
  np->tickets = p->tickets;

  /* Copy saved user registers. */
  *(np->trapframe) = *(p->trapframe);

  /* Cause fork to return 0 in the child. */
  np->trapframe->a0 = 0;

  /* Increment reference counts on open file descriptors. */
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);

  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

/* Pass p's abandoned children to init. */
void reparent(struct proc *p)
{
  for (struct proc *pp = proc; pp < &proc[NPROC]; pp++) {
    if (pp->parent == p) {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

/* Exit the current process. */
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  /* Close all open files. */
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      fileclose(p->ofile[fd]);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  reparent(p);

  wakeup(p->parent);
  
  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  sched();
  panic("zombie exit");
}

/* Wait for a child process to exit and return its pid. */
int wait(unsigned long addr)
{
  struct proc *pp, *p = myproc();
  int havekids, pid;

  acquire(&wait_lock);

  for (;;) {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      if (pp->parent == p) {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE) {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copy_to_user(p->pagetable, addr, (char *)&pp->xstate,
                                  sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p)) {
      release(&wait_lock);
      return -1;
    }
    
    // Wait for a child to exit.
    sleep(p, &wait_lock);
  }
}

/* Per-CPU process scheduler.
 * Each CPU calls scheduler() after setting itself up.
 * Scheduler never returns.  It loops, doing:
 *  - choose a process to run.
 *  - swtch to start running that process.
 *  - eventually that process transfers control
 *    via swtch back to the scheduler.
 */
void scheduler()
{
  unsigned long total_tickets = 0, winner;
  struct cpu *c = mycpu();
  int i, tickets[NPROC];
  struct proc *p;
  
  c->proc = 0;
  for (;;) {
    intr_on();

    for (i = 0; i < NPROC; i++) {
      if (proc[i].state != RUNNABLE) {
        continue;
      }
      total_tickets += proc[i].tickets;
      tickets[i] = total_tickets;
    }

    winner = rand() % total_tickets;

    for (i = 0; i < NPROC; i++) {
      if (tickets[i] > winner) {
        p = &proc[i];
        break;
      }
      if (i == NPROC - 1)
        p = &proc[i-1];
    }

    acquire(&p->lock);
    if (p->state == RUNNABLE) {
      p->state = RUNNING;
      c->proc = p;
      swtch(&c->context, &p->context);

      /* Process is done running for now. */
      c->proc = 0;
    }
    release(&p->lock);
  }
}

/* Switch to scheduler. */
void sched()
{
  struct proc *p = myproc();
  int intena;

  if (!holding(&p->lock))
    panic("sched p->lock");

  if (mycpu()->noff != 1)
    panic("sched locks");

  if (p->state == RUNNING)
    panic("sched running");

  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

/* Give up the CPU for one scheduling round. */
void yield()
{
  struct proc *p = myproc();

  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

/* A fork child's very first scheduling by scheduler() will swtch to forkret. */
void forkret()
{
  static int first = 1;

  release(&myproc()->lock);

  if (first) {
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

/* Atomically release lock and sleep on 'wait channel'. */
void sleep(void *channel, struct spinlock *lk)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  release(lk);

  p->channel = channel;
  p->state = SLEEPING;

  sched();

  p->channel = 0;

  /* Reacquire original lock. */
  release(&p->lock);
  acquire(lk);
}

/* Wake up all processes sleeping on channel. */
void wakeup(void *channel)
{
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    if (p != myproc()) {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->channel == channel)
        p->state = RUNNABLE;
      release(&p->lock);
    }
  }
}

/* Kill the process */
int kill(int pid)
{
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid) {
      p->killed = 1;
      if (p->state == SLEEPING)
        p->state = RUNNABLE;

      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }

  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;
  
  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);

  return k;
}

/* Copy to either a user address, or kernel address, depending on usr_dst. */
int either_copyout(bool user_dst, unsigned long dst, void *src, unsigned long len)
{
  if (user_dst)
    return copy_to_user(myproc()->pagetable, dst, src, len);

  memmove((char *)dst, src, len);

  return 0;
}

/* Copy from either a user address, or kernel address, depending on usr_src. */
int either_copyin(void *dst, bool user_src, unsigned long src, unsigned long len)
{
  if (user_src)
    return copy_from_user(myproc()->pagetable, dst, src, len);

  memmove(dst, (char*)src, len);

  return 0;
}

/* Print a process listing to console. */
void procdump()
{
  static char *states[] = {
  [UNUSED]    "unused",
  [USED]      "used",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}

void procinfo(struct pstat *ps)
{
  for (struct proc *p = &proc[0]; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      ps->tickets[&proc[NPROC] - p] = p->tickets;
      ps->ticks[&proc[NPROC] - p] = p->ticks;
      ps->pid[&proc[NPROC] - p] = p->pid;
      release(&p->lock);
    }
}