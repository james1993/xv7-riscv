struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  unsigned int dev;
  unsigned int blockno;
  struct sleeplock lock;
  unsigned int refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  unsigned char data[BSIZE];
};

