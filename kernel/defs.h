struct buf;
struct context;
struct file;
struct inode;
struct pipe;
struct proc;
struct spinlock;
struct sleeplock;
struct stat;
struct superblock;
struct pstat;

// bio.c
void            binit();
struct buf*     bread(unsigned int, unsigned int);
void            brelse(struct buf*);
void            bwrite(struct buf*);
void            bpin(struct buf*);
void            bunpin(struct buf*);

// console.c
void            console_init();
void            console_handle_irq(int);
void            console_put(int);

// exec.c
int             exec(char*, char**);

// file.c
struct file*    filealloc();
void            fileclose(struct file*);
struct file*    filedup(struct file*);
void            fileinit();
int             fileread(struct file*, unsigned long, int n);
int             filestat(struct file*, unsigned long addr);
int             filewrite(struct file*, unsigned long, int n);

// fs.c
void            fsinit(int);
int             dirlink(struct inode*, char*, unsigned int);
struct inode*   dirlookup(struct inode*, char*, unsigned int*);
struct inode*   ialloc(unsigned int, short);
struct inode*   idup(struct inode*);
void            iinit();
void            ilock(struct inode*);
void            iput(struct inode*);
void            iunlock(struct inode*);
void            iunlockput(struct inode*);
void            iupdate(struct inode*);
int             namecmp(const char*, const char*);
struct inode*   namei(char*);
struct inode*   nameiparent(char*, char*);
int             readi(struct inode*, int, unsigned long, unsigned int, unsigned int);
void            stati(struct inode*, struct stat*);
int             writei(struct inode*, bool, unsigned long, unsigned int, unsigned int);
void            itrunc(struct inode*);

// ramdisk.c
void            ramdiskinit();
void            ramdiskintr();
void            ramdiskrw(struct buf*);

// kalloc.c
void*           kalloc();
void            kfree(void *);
void            kalloc_init();

// log.c
void            initlog(int, struct superblock*);
void            log_write(struct buf*);
void            begin_op();
void            end_op();

// pipe.c
int             pipealloc(struct file**, struct file**);
void            pipeclose(struct pipe*, int);
int             piperead(struct pipe*, unsigned long, int);
int             pipewrite(struct pipe*, unsigned long, int);

// printf.c
void            printf(char*, ...);
void            panic(char*) __attribute__((noreturn));
void            printf_init();

// proc.c
int             cpuid();
void            exit(int);
int             fork();
int             growproc(int);
void            proc_mapstacks(unsigned long *);
unsigned long *     proc_pagetable(struct proc *);
void            proc_freepagetable(unsigned long *, unsigned long);
int             kill(int);
int             killed(struct proc*);
void            setkilled(struct proc*);
struct cpu*     mycpu();
struct cpu*     getmycpu();
struct proc*    myproc();
void            proc_init();
void            scheduler() __attribute__((noreturn));
void            sched();
void            sleep(void*, struct spinlock*);
void            userinit();
int             wait(unsigned long);
void            wakeup(void*);
void            yield();
int             either_copyout(bool user_dst, unsigned long dst, void *src, unsigned long len);
int             either_copyin(void *dst, bool user_src, unsigned long src, unsigned long len);
void            procdump();
void            procinfo(struct pstat *);

// swtch.S
void            swtch(struct context*, struct context*);

// spinlock.c
void            acquire(struct spinlock*);
int             holding(struct spinlock*);
void            initlock(struct spinlock*);
void            release(struct spinlock*);
void            push_off();
void            pop_off();

// sleeplock.c
void            acquiresleep(struct sleeplock*);
void            releasesleep(struct sleeplock*);
int             holdingsleep(struct sleeplock*);
void            initsleeplock(struct sleeplock*);

// string.c
int             memcmp(const void*, const void*, unsigned int);
void*           memmove(void*, const void*, unsigned int);
void*           memset(void*, int, unsigned int);
char*           safestrcpy(char*, const char*, int);
int             strlen(const char*);
int             strncmp(const char*, const char*, unsigned int);
char*           strncpy(char*, const char*, int);

// syscall.c
void            argint(int, int*);
int             argstr(int, char*, int);
void            argaddr(int, unsigned long *);
int             fetchstr(unsigned long, char*, int);
int             fetchaddr(unsigned long, unsigned long*);
void            syscall();

// trap.c
extern unsigned int     ticks;
extern unsigned int     readcount;
void            trapinit();
void            trapinithart();
extern struct spinlock tickslock;
extern struct spinlock readcountlock;
void            usertrapret();

// uart.c
void            uart_init();
void            uart_handle_irq();
void            uart_put(int);
void            uart_put_nosleep(int);

// vm.c
void            kvm_init();
void            kvm_init_hart();
void            kvm_map(unsigned long *, unsigned long, unsigned long, unsigned long, int);
int             mappages(unsigned long *, unsigned long, unsigned long, unsigned long, int);
void            uvm_first(unsigned long *, unsigned char *, unsigned int);
unsigned long          uvm_alloc(unsigned long *, unsigned long, unsigned long, int);
unsigned long          uvm_dealloc(unsigned long *, unsigned long, unsigned long);
int             uvm_copy(unsigned long *, unsigned long *, unsigned long);
void            uvm_free(unsigned long *, unsigned long);
void            uvm_unmap(unsigned long *, unsigned long, unsigned long, int);
void            uvm_clear(unsigned long *, unsigned long);
unsigned long          walkaddr(unsigned long *, unsigned long);
int             copy_to_user(unsigned long *, unsigned long, char *, unsigned long);
int             copy_from_user(unsigned long *, char *, unsigned long, unsigned long);
int             copyin_str(unsigned long *, char *, unsigned long, unsigned long);

// plic.c
void            plicinit();
void            plicinithart();
int             plic_claim();
void            plic_complete(int);

// virtio_disk.c
void            virtio_disk_init();
void            virtio_disk_rw(struct buf *, int);
void            virtio_disk_intr();

// number of elements in fixed-size array
#define NELEM(x) (sizeof(x)/sizeof((x)[0]))
