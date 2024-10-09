#include "riscv.h"
#include "defs.h"

volatile static bool started = 0;

/* start() jumps here in supervisor mode on all CPUs. */
void main()
{
  if (cpuid() == 0) {
    console_init();
    printf_init();
    kalloc_init();
    kvm_init();
    proc_init();
    trap_init();
    plic_init();
    bufcache_init();
    inode_init();
    file_init();
    virtio_disk_init();
    user_init();
    started = true;
  } else {
    while (!started);
  }

  /* Do on all CPUs */
  kvm_init_hart();
  trap_init_hart();
  plic_init_hart();

  scheduler();
}
