#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
struct spinlock readcountlock;
unsigned int ticks;
unsigned int readcount;

extern char trampoline[], uservec[], userret[];

/* In kernelvec.S, calls kerneltrap(). */
void kernelvec();

extern int devintr();

void trap_init()
{
  initlock(&tickslock);
}

/* Set up to take exceptions and traps while in the kernel. */
void trap_init_hart()
{
  w_stvec((unsigned long)kernelvec);
}

/* Handle an interrupt, exception, or system call from user space. */
void usertrap()
{
  struct proc *p = myproc();
  int which_dev = 0;

  if ((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  /* Send interrupts and exceptions to kerneltrap(), since we're now in the kernel. */
  w_stvec((unsigned long)kernelvec);
  
  /* save user PC. */
  p->trapframe->epc = r_sepc();
  
  if (r_scause() == 8) {
    /* System call */

    if (killed(p))
      exit(-1);

    /* sepc points to the ecall instruction, but we want to return to the next instruction. */
    p->trapframe->epc += 4;

    intr_on();

    syscall();
  } else if (!(which_dev = devintr())) {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if (killed(p))
    exit(-1);

  /* Give up the CPU if this is a timer interrupt. */
  if (which_dev == 2)
    yield();

  usertrapret();
}

/* Return to user space */
void usertrapret()
{
  unsigned long trampoline_userret = TRAMPOLINE + (userret - trampoline),
      trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  struct proc *p = myproc();

  intr_off();

  w_stvec(trampoline_uservec);

  /* Set up trapframe values for when the process next traps into the kernel. */
  p->trapframe->kernel_satp = r_satp();
  p->trapframe->kernel_sp = p->kstack + PGSIZE;
  p->trapframe->kernel_trap = (unsigned long)usertrap;
  p->trapframe->kernel_hartid = r_tp();
  
  /* set Previous Privilege mode to User. */
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; /* Clear SPP to 0 for user mode */
  x |= SSTATUS_SPIE; /* Enable interrupts in user mode */
  w_sstatus(x);

  /* set S Exception Program Counter to the saved user PC. */
  w_sepc(p->trapframe->epc);

  /* Jump to userret, which switches to the user page table,
     restores user registers, and switches to user mode with sret. */
  ((void (*)(unsigned long))trampoline_userret)(MAKE_SATP(p->pagetable));
}

/* Interrupts and exceptions from kernel code go here. */
void kerneltrap()
{
  unsigned long sepc = r_sepc(), sstatus = r_sstatus(), scause = r_scause();
  int which_dev = 0;
  
  if (!(sstatus & SSTATUS_SPP))
    panic("kerneltrap: not from supervisor mode");

  if (intr_get())
    panic("kerneltrap: interrupts enabled");

  if (!(which_dev = devintr())) {
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  /* Give up the CPU if this is a timer interrupt. */
  if (which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  /* yield() may have caused some traps to occur, so restore trap registers */
  w_sepc(sepc);
  w_sstatus(sstatus);
}

static void clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

/* Check if it's an external interrupt or software interrupt, and handle it.
 * Returns 2 if timer interrupt, 1 if other device, 0 if not recognized.
 */
int devintr()
{
  unsigned long scause = r_scause();
  struct proc *p = myproc();
  int irq;

  if ((scause & 0x8000000000000000L) && (scause & 0xff) == 9) {
    /* Device interrupt */
    irq = plic_claim();

    if (irq == UART0_IRQ) {
      uart_handle_irq();
    } else if (irq == VIRTIO0_IRQ) {
      virtio_disk_intr();
    } else if (irq) {
      printf("unexpected device interrupt irq=%d\n", irq);
    }

    if (irq)
      plic_complete(irq);

    return 1;
  } else if (scause == 0x8000000000000001L) {
    /* Timer interrupt */
    if (p && p->alarmticks != 0 && ++p->ticks % p->alarmticks == 0)
      p->alarmhandler();

    if (cpuid() == 0)
      clockintr();

    w_sip(r_sip() & ~2);

    return 2;
  }

  return 0;
}

