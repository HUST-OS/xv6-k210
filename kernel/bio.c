// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.



#include "include/types.h"
#include "include/param.h"
#include "include/spinlock.h"
#include "include/sleeplock.h"
#include "include/riscv.h"
#include "include/buf.h"
#include "include/sdcard.h"
#include "include/printf.h"
#include "include/disk.h"
#include "include/kmalloc.h"

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;

#define BCACHE_TABLE_SIZE 17
#define __hash_idx(idx) ((idx) % BCACHE_TABLE_SIZE)

static struct spinlock bcachelock;
static struct buf *bcache[BCACHE_TABLE_SIZE];

void
binit(void)
{
	initlock(&bcachelock, "bcache");
	for (int i = 0; i < BCACHE_TABLE_SIZE; i++) {
		bcache[i] = NULL;
	}
	// // Create linked list of buffers
	// bcache.head.prev = &bcache.head;
	// bcache.head.next = &bcache.head;
	// for(b = bcache.buf; b < bcache.buf+NBUF; b++){
	// 	b->refcnt = 0;
	// 	b->sectorno = ~0;
	// 	b->dev = ~0;
	// 	b->next = bcache.head.next;
	// 	b->prev = &bcache.head;
	// 	initsleeplock(&b->lock, "buffer");
	// 	bcache.head.next->prev = b;
	// 	bcache.head.next = b;
	// }
	#ifdef DEBUG
	printf("binit\n");
	#endif
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint sectorno)
{
	struct buf *b;
	struct buf **bp;
	int idx = __hash_idx(sectorno);

	acquire(&bcachelock);
	// Is the block already cached?
	for(bp = &bcache[idx]; *bp != NULL; bp = &(*bp)->next){
		b = *bp;
		if(b->dev == dev && b->sectorno == sectorno){
			*bp = b->next;
			b->next = bcache[idx];
			bcache[idx] = b;
			b->refcnt++;
			release(&bcachelock);
			return b;
		}
	}

	if ((b = kmalloc(sizeof(struct buf))) == NULL) {
		panic("bget: no buffers");	// TODO: evict some bufs or fall asleep
		release(&bcachelock);
		return NULL;
	}
	b->dev = dev;
	b->sectorno = sectorno;
	b->valid = 0;
	b->refcnt = 1;
	initsleeplock(&b->lock, "buffer");

	b->next = bcache[idx];
	bcache[idx] = b;

	release(&bcachelock);
	return b;
	// Not cached.
	// Recycle the least recently used (LRU) unused buffer.
	// for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
	// 	if(b->refcnt == 0) {
	// 		b->dev = dev;
	// 		b->sectorno = sectorno;
	// 		b->valid = 0;
	// 		b->refcnt = 1;
	// 		release(&bcache.lock);
	// 		acquiresleep(&b->lock);
	// 		return b;
	// 	}
	// }
	// panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf* 
bread(uint dev, uint sectorno) {
	struct buf *b;

	if ((b = bget(dev, sectorno)) == NULL) {
		return NULL;
	}
	acquiresleep(&b->lock);
	if (!b->valid) {
		disk_read(b);
		b->valid = 1;
	}

	return b;
}

// Write b's contents to disk.  Must be locked.
void 
bwrite(struct buf *b) {
	if(!holdingsleep(&b->lock))
		panic("bwrite");
	disk_write(b);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
	if(!holdingsleep(&b->lock))
		panic("brelse");

	releasesleep(&b->lock);

	acquire(&bcachelock);
	b->refcnt--;
	// if (b->refcnt == 0) {
	// 	// no one is waiting for it.
	// 	b->next->prev = b->prev;
	// 	b->prev->next = b->next;
	// 	b->next = bcache.head.next;
	// 	b->prev = &bcache.head;
	// 	bcache.head.next->prev = b;
	// 	bcache.head.next = b;
	// }
	
	release(&bcachelock);
}

void bprint()
{
	acquire(&bcachelock);
	printf("\nbuf cache:\n");
	for (int i = 0; i < BCACHE_TABLE_SIZE; i++) {
		struct buf *b = bcache[i];
		if (b == NULL) { continue; }
		printf("[%d] ", i);
		for (; b != NULL; b = b->next) {
			printf("%d ", b->sectorno);
		}
		printf("\n");
	}
	release(&bcachelock);
}
