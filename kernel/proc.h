// Saved registers for kernel context switches.
struct context {
  unsigned long ra;
  unsigned long sp;

  // callee-saved
  unsigned long s0;
  unsigned long s1;
  unsigned long s2;
  unsigned long s3;
  unsigned long s4;
  unsigned long s5;
  unsigned long s6;
  unsigned long s7;
  unsigned long s8;
  unsigned long s9;
  unsigned long s10;
  unsigned long s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ unsigned long kernel_satp;   // kernel page table
  /*   8 */ unsigned long kernel_sp;     // top of process's kernel stack
  /*  16 */ unsigned long kernel_trap;   // usertrap()
  /*  24 */ unsigned long epc;           // saved user program counter
  /*  32 */ unsigned long kernel_hartid; // saved kernel tp
  /*  40 */ unsigned long ra;
  /*  48 */ unsigned long sp;
  /*  56 */ unsigned long gp;
  /*  64 */ unsigned long tp;
  /*  72 */ unsigned long t0;
  /*  80 */ unsigned long t1;
  /*  88 */ unsigned long t2;
  /*  96 */ unsigned long s0;
  /* 104 */ unsigned long s1;
  /* 112 */ unsigned long a0;
  /* 120 */ unsigned long a1;
  /* 128 */ unsigned long a2;
  /* 136 */ unsigned long a3;
  /* 144 */ unsigned long a4;
  /* 152 */ unsigned long a5;
  /* 160 */ unsigned long a6;
  /* 168 */ unsigned long a7;
  /* 176 */ unsigned long s2;
  /* 184 */ unsigned long s3;
  /* 192 */ unsigned long s4;
  /* 200 */ unsigned long s5;
  /* 208 */ unsigned long s6;
  /* 216 */ unsigned long s7;
  /* 224 */ unsigned long s8;
  /* 232 */ unsigned long s9;
  /* 240 */ unsigned long s10;
  /* 248 */ unsigned long s11;
  /* 256 */ unsigned long t3;
  /* 264 */ unsigned long t4;
  /* 272 */ unsigned long t5;
  /* 280 */ unsigned long t6;
};

enum procstate { UNUSED, USED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *channel;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  unsigned long kstack;               // Virtual address of kernel stack
  unsigned long sz;                   // Size of process memory (bytes)
  unsigned long * pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  int tickets;                 // Tickets to the lottery
  int alarmticks;              // Alarm interval
  void (*alarmhandler)();      // Alarm handler
  int ticks;                   // Ticks passed
};
