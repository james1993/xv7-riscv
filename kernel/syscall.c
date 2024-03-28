#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"

// Fetch the unsigned long at addr from the current process.
int
fetchaddr(unsigned long addr, unsigned long *ip)
{
  struct proc *p = myproc();
  if (addr >= p->sz || addr+sizeof(unsigned long) > p->sz || addr == 0) // both tests needed, in case of overflow
    return -1;
  if (copy_from_user(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(unsigned long addr, char *buf, int max)
{
  struct proc *p = myproc();
  if (copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return strlen(buf);
}

static unsigned long
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
void
argint(int n, int *ip)
{
  *ip = argraw(n);
}

// Retrieve an argument as a pointer.
void
argaddr(int n, unsigned long *ip)
{
  *ip = argraw(n);
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  unsigned long addr;
  argaddr(n, &addr);
  return fetchstr(addr, buf, max);
}

// Prototypes for the functions that handle system calls.
extern unsigned long sys_fork();
extern unsigned long sys_exit();
extern unsigned long sys_wait();
extern unsigned long sys_pipe();
extern unsigned long sys_read();
extern unsigned long sys_kill();
extern unsigned long sys_exec();
extern unsigned long sys_fstat();
extern unsigned long sys_chdir();
extern unsigned long sys_dup();
extern unsigned long sys_getpid();
extern unsigned long sys_sbrk();
extern unsigned long sys_sleep();
extern unsigned long sys_uptime();
extern unsigned long sys_open();
extern unsigned long sys_write();
extern unsigned long sys_mknod();
extern unsigned long sys_unlink();
extern unsigned long sys_link();
extern unsigned long sys_mkdir();
extern unsigned long sys_close();
extern unsigned long sys_readcount();
extern unsigned long sys_alarm();
extern unsigned long sys_settickets();
extern unsigned long sys_getpinfo();

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static unsigned long (*syscalls[])() = {
[SYS_fork]	sys_fork,
[SYS_exit]	sys_exit,
[SYS_wait]	sys_wait,
[SYS_pipe]	sys_pipe,
[SYS_read]	sys_read,
[SYS_kill]	sys_kill,
[SYS_exec]	sys_exec,
[SYS_fstat]	sys_fstat,
[SYS_chdir]	sys_chdir,
[SYS_dup]	sys_dup,
[SYS_getpid]	sys_getpid,
[SYS_sbrk]	sys_sbrk,
[SYS_sleep]	sys_sleep,
[SYS_uptime]	sys_uptime,
[SYS_open]	sys_open,
[SYS_write]	sys_write,
[SYS_mknod]	sys_mknod,
[SYS_unlink]	sys_unlink,
[SYS_link]	sys_link,
[SYS_mkdir]	sys_mkdir,
[SYS_close]	sys_close,
[SYS_readcount]	sys_readcount,
[SYS_alarm] sys_alarm,
[SYS_settickets] sys_settickets,
[SYS_getpinfo] sys_getpinfo,
};

#ifdef SYSCALL_TRACE
static char *syscall_names[] = {
  "fork",
  "exit",
  "wait",
  "pipe",
  "read",
  "kill",
  "exec",
  "fstat",
  "chdir",
  "dup",
  "getpid",
  "sbrk",
  "sleep",
  "uptime",
  "open",
  "write",
  "mknod"
  "unlink",
  "link",
  "mkdir",
  "close",
  "readcount",
  "alarm",
  "settickets",
  "getpinfo",
};
#endif

void
syscall()
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if (num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
#ifdef SYSCALL_TRACE
    printf("%s -> %d\n", syscall_names[num-1], p->trapframe->a0);
#endif
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
