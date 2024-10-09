#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();

/* Allocate stack space for each CPU that the kernel will run on */
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

/* Scratch area per CPU for timer interrupts. */
unsigned long timer_scratch[NCPU][5];
/* assembly code in kernelvec.S for timer interrupt. */
extern void timervec();

/* entry.S jumps here in machine mode on stack0. */
void start()
{
  int id = r_mhartid(), interval = 1000000;
  unsigned long *scratch = &timer_scratch[id][0];

  /* Set Previous Privilege mode to Supervisor, so that we will
   * return into supervisor mode */
  w_mstatus((r_mstatus() & ~MSTATUS_MPP_MASK) | MSTATUS_MPP_S);

  /* Set Exception Program Counter to main, so that we will return
   * into main() */
  w_mepc((unsigned long)main);

  /* Traps by default go to highest privelege (machine mode).
   * Delegate all interrupts and exceptions to Supervisor mode. */
  w_medeleg(0xffff);
  w_mideleg(0xffff);
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  /* Give supervisor mode access to all of physical memory. */
  w_pmpaddr0(0x3fffffffffffffull);
  w_pmpcfg0(0xf);

  /* Arrange to receive timer interrupts. They will arrive in machine mode at
   * timervec in kernelvec.S, which turns them into software interrupts for
   * devintr() in trap.c. */

  /* Configure clock interrupt to occur every ~1/10th second. */
  *(unsigned long *)CLINT_MTIMECMP(id) = *(unsigned long*)CLINT_MTIME + interval;

  /* prepare information in scratch[] for timervec.
   * scratch[0..2] : space for timervec to save registers.
   * scratch[3] : address of CLINT MTIMECMP register.
   * scratch[4] : desired interval (in cycles) between timer interrupts.
   * gives us ability to save the state and restore it while we execute
   * interrupts */
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((unsigned long)scratch);

  /* Set the machine-mode trap handler. */
  w_mtvec((unsigned long)timervec);

  /* Enable machine-mode interrupts. */
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  /* Enable machine-mode timer interrupts. */
  w_mie(r_mie() | MIE_MTIE);

  /* Keep each CPU id in its tp register */
  w_tp(id);

  /* Machine mode return */
  __asm__ volatile("mret");
}
