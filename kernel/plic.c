/* The riscv Platform Level Interrupt Controller (PLIC). */

#include "memlayout.h"
#include "defs.h"

/* Enable IRQs */
void plic_init()
{
  *(unsigned int*)(PLIC + UART0_IRQ*4) = 1;
  *(unsigned int*)(PLIC + VIRTIO0_IRQ*4) = 1;
}

/* Enable IRQs for this hart */
void plic_init_hart()
{
  int hart = cpuid();
  
  *(unsigned int*)PLIC_SENABLE(hart) = (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);
  *(unsigned int*)PLIC_SPRIORITY(hart) = 0;
}

/* Ask PLIC which interrupt we should serve. */
int plic_claim()
{
  return *(unsigned int*)PLIC_SCLAIM(cpuid());
}

/* Tell the PLIC we've served this IRQ. */
void plic_complete(int irq)
{
  *(unsigned int*)PLIC_SCLAIM(cpuid()) = irq;
}
