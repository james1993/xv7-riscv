#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "pstat.h"

unsigned long sys_exit(void)
{
	int n;

	argint(0, &n);
	exit(n);

	return 0;  // not reached
}

unsigned long sys_getpid(void)
{
	return myproc()->pid;
}

unsigned long sys_fork(void)
{
	return fork();
}

unsigned long sys_wait(void)
{
	unsigned long p;

	argaddr(0, &p);

	return wait(p);
}

unsigned long sys_sbrk(void)
{
	unsigned long addr;
	int n;

	argint(0, &n);
	addr = myproc()->sz;
	if (growproc(n) < 0)
		return -1;

	return addr;
}

unsigned long sys_sleep(void)
{
	int n;
	unsigned int ticks0;

	argint(0, &n);
	acquire(&tickslock);
	ticks0 = ticks;
	while (ticks - ticks0 < n) {
		if (killed(myproc())) {
			release(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	release(&tickslock);
	return 0;
}

unsigned long sys_kill(void)
{
	int pid;

	argint(0, &pid);
	return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
unsigned long sys_uptime(void)
{
	unsigned int xticks;

	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);

	return xticks;
}

// total times processes have called the read() system
// call
unsigned long sys_readcount(void)
{
	unsigned int xreadcount;

	acquire(&readcountlock);
	xreadcount = readcount;
	release(&readcountlock);

	return xreadcount;
}

// after every n ticks of CPU time that the program consumes, call function fn
unsigned long sys_alarm(void)
{
	int n;
	unsigned long fn;

	argint(0, &n);
	argaddr(1, &fn);
	myproc()->alarmticks = n;
	myproc()->alarmhandler = (void (*)(void))fn;

	return 0;
}

unsigned long sys_settickets(int number)
{
	int n;

	argint(0, &n);
	if (n < 1)
		return -1;

	myproc()->tickets = n;

	return 0;
}

unsigned long sys_getpinfo(struct pstat *ps)
{
	unsigned long p;
	argaddr(0, &p);

	ps = (struct pstat *)p;
	procinfo(ps);

	return 0;
}