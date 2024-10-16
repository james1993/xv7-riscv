#ifndef __ASSEMBLER__

// which hart (core) is this?
static inline unsigned long
r_mhartid()
{
  unsigned long x;
  __asm__ volatile("csrr %0, mhartid" : "=r" (x) );
  return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)    // machine-mode interrupt enable.

static inline unsigned long
r_mstatus()
{
  unsigned long x;
  __asm__ volatile("csrr %0, mstatus" : "=r" (x) );
  return x;
}

static inline void 
w_mstatus(unsigned long x)
{
  __asm__ volatile("csrw mstatus, %0" : : "r" (x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_mepc(unsigned long x)
{
  __asm__ volatile("csrw mepc, %0" : : "r" (x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline unsigned long
r_sstatus()
{
  unsigned long x;
  __asm__ volatile("csrr %0, sstatus" : "=r" (x) );
  return x;
}

static inline void 
w_sstatus(unsigned long x)
{
  __asm__ volatile("csrw sstatus, %0" : : "r" (x));
}

// Supervisor Interrupt Pending
static inline unsigned long
r_sip()
{
  unsigned long x;
  __asm__ volatile("csrr %0, sip" : "=r" (x) );
  return x;
}

static inline void 
w_sip(unsigned long x)
{
  __asm__ volatile("csrw sip, %0" : : "r" (x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software
static inline unsigned long
r_sie()
{
  unsigned long x;
  __asm__ volatile("csrr %0, sie" : "=r" (x) );
  return x;
}

static inline void 
w_sie(unsigned long x)
{
  __asm__ volatile("csrw sie, %0" : : "r" (x));
}

// Machine-mode Interrupt Enable
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software
static inline unsigned long
r_mie()
{
  unsigned long x;
  __asm__ volatile("csrr %0, mie" : "=r" (x) );
  return x;
}

static inline void 
w_mie(unsigned long x)
{
  __asm__ volatile("csrw mie, %0" : : "r" (x));
}

// supervisor exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void 
w_sepc(unsigned long x)
{
  __asm__ volatile("csrw sepc, %0" : : "r" (x));
}

static inline unsigned long
r_sepc()
{
  unsigned long x;
  __asm__ volatile("csrr %0, sepc" : "=r" (x) );
  return x;
}

// Machine Exception Delegation
static inline unsigned long
r_medeleg()
{
  unsigned long x;
  __asm__ volatile("csrr %0, medeleg" : "=r" (x) );
  return x;
}

static inline void 
w_medeleg(unsigned long x)
{
  __asm__ volatile("csrw medeleg, %0" : : "r" (x));
}

// Machine Interrupt Delegation
static inline unsigned long
r_mideleg()
{
  unsigned long x;
  __asm__ volatile("csrr %0, mideleg" : "=r" (x) );
  return x;
}

static inline void 
w_mideleg(unsigned long x)
{
  __asm__ volatile("csrw mideleg, %0" : : "r" (x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void 
w_stvec(unsigned long x)
{
  __asm__ volatile("csrw stvec, %0" : : "r" (x));
}

static inline unsigned long
r_stvec()
{
  unsigned long x;
  __asm__ volatile("csrr %0, stvec" : "=r" (x) );
  return x;
}

// Machine-mode interrupt vector
static inline void 
w_mtvec(unsigned long x)
{
  __asm__ volatile("csrw mtvec, %0" : : "r" (x));
}

// Physical Memory Protection
static inline void
w_pmpcfg0(unsigned long x)
{
  __asm__ volatile("csrw pmpcfg0, %0" : : "r" (x));
}

static inline void
w_pmpaddr0(unsigned long x)
{
  __asm__ volatile("csrw pmpaddr0, %0" : : "r" (x));
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((unsigned long)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void 
w_satp(unsigned long x)
{
  __asm__ volatile("csrw satp, %0" : : "r" (x));
}

static inline unsigned long
r_satp()
{
  unsigned long x;
  __asm__ volatile("csrr %0, satp" : "=r" (x) );
  return x;
}

static inline void 
w_mscratch(unsigned long x)
{
  __asm__ volatile("csrw mscratch, %0" : : "r" (x));
}

// Supervisor Trap Cause
static inline unsigned long
r_scause()
{
  unsigned long x;
  __asm__ volatile("csrr %0, scause" : "=r" (x) );
  return x;
}

// Supervisor Trap Value
static inline unsigned long
r_stval()
{
  unsigned long x;
  __asm__ volatile("csrr %0, stval" : "=r" (x) );
  return x;
}

// Machine-mode Counter-Enable
static inline void 
w_mcounteren(unsigned long x)
{
  __asm__ volatile("csrw mcounteren, %0" : : "r" (x));
}

static inline unsigned long
r_mcounteren()
{
  unsigned long x;
  __asm__ volatile("csrr %0, mcounteren" : "=r" (x) );
  return x;
}

// machine-mode cycle counter
static inline unsigned long
r_time()
{
  unsigned long x;
  __asm__ volatile("csrr %0, time" : "=r" (x) );
  return x;
}

// enable device interrupts
static inline void
intr_on()
{
  w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void
intr_off()
{
  w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int
intr_get()
{
  unsigned long x = r_sstatus();
  return (x & SSTATUS_SIE) != 0;
}

static inline unsigned long
r_sp()
{
  unsigned long x;
  __asm__ volatile("mv %0, sp" : "=r" (x) );
  return x;
}

// read and write tp, the thread pointer, which xv6 uses to hold
// this core's hartid (core number), the index into cpus[].
static inline unsigned long
r_tp()
{
  unsigned long x;
  __asm__ volatile("mv %0, tp" : "=r" (x) );
  return x;
}

static inline void 
w_tp(unsigned long x)
{
  __asm__ volatile("mv tp, %0" : : "r" (x));
}

static inline unsigned long
r_ra()
{
  unsigned long x;
  __asm__ volatile("mv %0, ra" : "=r" (x) );
  return x;
}

// flush the TLB.
static inline void
sfence_vma()
{
  // the zero, zero means flush all TLB entries.
  __asm__ volatile("sfence.vma zero, zero");
}

#endif // __ASSEMBLER__

#define PGSIZE 4096 // bytes per page
#define PGSHIFT 12  // bits of offset within a page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // user can access

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((unsigned long)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// extract the three 9-bit page table indices from a virtual address.
#define PXMASK          0x1FF // 9 bits
#define PXSHIFT(level)  (PGSHIFT+(9*(level)))
#define PX(level, va) ((((unsigned long) (va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
