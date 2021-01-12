#define BSIZE 512

struct buf {
  int valid;
  int disk;		// does disk "own" buf? 
  uint dev;
  uint sectorno;	// sector number 
  uint blockno;	// old should be deleted in the future
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev;
  struct buf *next;
  uchar data[BSIZE];
};