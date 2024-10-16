struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe; // FD_PIPE
  struct inode *ip;  // FD_INODE and FD_DEVICE
  unsigned int off;          // FD_INODE
  short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((unsigned int)((m)<<16| (n)))

// in-memory copy of an inode
struct inode {
  unsigned int dev;           // Device number
  unsigned int inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  unsigned int size;
  unsigned int addrs[NDIRECT+1];
};

// Map major device number to device functions.
struct devsw {
  int (*read)(unsigned long, int);
  int (*write)(unsigned long, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
