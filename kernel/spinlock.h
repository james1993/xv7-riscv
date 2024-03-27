// Mutual exclusion lock.
struct spinlock {
  unsigned int locked;       // Is the lock held?
  struct cpu *cpu;   // The cpu holding the lock.
};

