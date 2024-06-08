#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void main()
{
  if (cpuid() == 0) {
    console_init();
    printf_init();
    printf("\nxv7 kernel is booting\n\n");
    kalloc_init();
    kvm_init();
    kvm_init_hart();
    proc_init();
    trap_init();
    trap_init_hart();
    plic_init();
    plic_init_hart();
    bufcache_init();
    inode_init();
    file_init();
    virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    __sync_synchronize();
    started = 1;
  } else {
    while (started == 0);
    __sync_synchronize();
    printf("hart %d starting\n", cpuid());
    kvm_init_hart();    // turn on paging
    trap_init_hart();   // install kernel trap vector
    plic_init_hart();   // ask PLIC for device interrupts
  }

  scheduler();        
}
