/* Functions for system calls that involve file descriptors. */

#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"

struct file_table {
  struct spinlock lock;
  struct file file[NFILE];
};

struct file_table ftable;
struct devsw devsw[NDEV];

void file_init()
{
  initlock(&ftable.lock);
}

struct file *file_alloc()
{
  struct file *f;

  acquire(&ftable.lock);

  for (f = ftable.file; f < ftable.file + NFILE; f++) {
    if (f->ref == 0) {
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }

  release(&ftable.lock);

  return 0;
}

/* Increment ref count for file f. */
struct file* file_dup(struct file *f)
{
  acquire(&ftable.lock);
  f->ref++;
  release(&ftable.lock);

  return f;
}

/* Decrement ref count, close if 0. */
void file_close(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if (--f->ref > 0) {
    release(&ftable.lock);
    return;
  }

  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if (ff.type == FD_PIPE) {
    pipeclose(ff.pipe, ff.writable);
  } else if (ff.type == FD_INODE || ff.type == FD_DEVICE) {
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct status.
int file_status(struct file *f, unsigned long addr)
{
  struct proc *p = myproc();
  struct status st;
  
  if (f->type == FD_INODE || f->type == FD_DEVICE) {
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if (copy_to_user(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, unsigned long addr, int n)
{
  int r = 0;

  if (f->readable == 0)
    return -1;

  if (f->type == FD_PIPE) {
    r = piperead(f->pipe, addr, n);
  } else if (f->type == FD_DEVICE) {
    if (f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(addr, n);
  } else if (f->type == FD_INODE) {
    ilock(f->ip);
    if ((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, unsigned long addr, int n)
{
  int r, ret = 0;

  if (f->writable == 0)
    return -1;

  if (f->type == FD_PIPE) {
    ret = pipewrite(f->pipe, addr, n);
  } else if (f->type == FD_DEVICE) {
    if (f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(addr, n);
  } else if (f->type == FD_INODE) {
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while (i < n) {
      int n1 = n - i;
      if (n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if (r != n1) {
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

