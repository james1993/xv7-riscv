/* The buffer cache is a linked list of buf structures holding
 * cached copies of disk block contents.
 *
 * Interface:
 * * To get a buffer for a particular disk block, call bufcache_read.
 * * After changing buffer data, call bufcache_write to write it to disk.
 * * When done with the buffer, call bufcache_release.
 * * Only one process at a time can use a buffer, so do not keep them longer than necessary.
 */

#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct buffer_cache {
  struct spinlock lock;
  struct buf buf[NBUF];

  /* Linked list of all buffers, through prev/next.
   * LRU: head.next is most recent, head.prev is least.
   */
  struct buf head;
};

struct buffer_cache bcache;

void bufcache_init()
{
  struct buf *b;

  initlock(&bcache.lock);

  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++) {
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock);
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

/* Look through buffer cache for block on file system root disk.
 * If not found, allocate a buffer. In either case, return locked buffer.
 */
static struct buf* bufcache_get(unsigned int blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  /* Is the block already cached? */
  for (b = bcache.head.next; b != &bcache.head; b = b->next) {
    if (b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  /* Not cached. Recycle the least recently used (LRU) unused buffer. */
  for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
    if (b->refcnt == 0) {
      b->blockno = blockno;
      b->valid = false;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bufcache_get: no buffers");
}

/* Return a buffer with the contents of the indicated block. */
struct buf *bufcache_read(unsigned int blockno)
{
  struct buf *b = bufcache_get(blockno);

  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = true;
  }

  return b;
}

/* Write buffer contents to disk. Must be locked. */
void bufcache_write(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bufcache_write");

  virtio_disk_rw(b, 1);
}

/* Release a locked buffer. Move to the head of the LRU cache. */
void bufcache_release(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bufcache_release");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    /* no one is waiting for it. */
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void bufcache_pin(struct buf *b)
{
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void bufcache_unpin(struct buf *b)
{
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


