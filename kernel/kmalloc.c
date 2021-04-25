/**
 * A small-mem allocator implement inspired by the slab-mechanism of Linux.
 * However, our design is much easier and less considered. We ignore the
 * effect of the hardware-cache by excluding the CPU struct in the kmem_cache.
 */

#include "include/types.h"
#include "include/pm.h"
#include "include/kmalloc.h"
#include "include/spinlock.h"
#include "include/printf.h"

#include "include/proc.h"
#include "include/buf.h"
#include "include/fat32.h"
#include "include/file.h"

#define KMEM_OBJ_MIN_SIZE   32
#define KMEM_OBJ_MAX_COUNT  124   // with MIN_SIZE, there are most 123 objs in a node, and next[0] for special use

struct kmem_node {
	struct kmem_node      *list;    // link to next partial node
	void                  *objs;    // pointer to the first object
	ushort                size;     // size of the objs (rounds up to 8), all objs in the some node are equal-sized
	uchar                 capacity; // vary from different object kinds
	uchar                 nfree;    // number of free object slots
	uchar                 next[KMEM_OBJ_MAX_COUNT]; // form a linked list list FAT table, must be placed at last
};

struct kmem_allocator {
	struct spinlock       lock;
	struct kmem_node      *list;
};

static struct spinlock alloc_lock;
static struct kmem_allocator *allocs[5];   // temp
static struct kmem_allocator alloc_min;

void kmallocinit()
{
	if (sizeof(struct kmem_allocator) > KMEM_OBJ_MIN_SIZE)
		panic("kmallocinit");

	initlock(&alloc_lock, "allocs");
	initlock(&alloc_min.lock, "allocmin");
	alloc_min.list = NULL;
	allocs[0] = &alloc_min;
	for (int i = 1; i < NELEM(allocs); i++) {
		allocs[i] = NULL;
	}
}

static int get_alloc_idx(uint size)
{
	if (size & 7)
		panic("get_alloc_idx: size not align 8");
	switch (size) {           		// temporary test impl
		case KMEM_OBJ_MIN_SIZE:
			return 0;
		case sizeof(struct proc):
			return 1;
		case sizeof(struct buf):
			return 2;
		case sizeof(struct dirent):
			return 3;
		case sizeof(struct file):
			return 4;
		default:
			return -1;
	}
}

// Called by kfree, to get the allocators to which the nodes belong
static struct kmem_allocator *get_allocator(uint size)
{
	int index = get_alloc_idx(size);
	if (index < 0 || allocs[index] == NULL)
		panic("get_allocator");
	return allocs[index];
}

static struct kmem_node *alloc_node(uint size)
{
	struct kmem_node *kn;
	if ((kn = allocpage()) == NULL) { return NULL; }
	
	kn->size = size;
	int freebytes = PGSIZE - (sizeof(struct kmem_node) - sizeof(kn->next)) - 1;  // '1' refer to next[0]
	// The count of objs may not fill up all the next[] uchar arr, so we can make good use of the spare space.
	// Solve this inequation:
	//     (freebytes - next_size) / size >= capacity
	// where next_size == capacity.
	kn->capacity = freebytes / (size + 1);
	int offset = (PGSIZE - freebytes + kn->capacity + 7) & ~7;   // align to 8, it is safe
	kn->objs = (void *)((uint64)kn + offset);

	kn->nfree = kn->capacity;
	kn->list = NULL;
	for (int i = 0; i < kn->capacity; i++) {  // form a link list like FAT
		kn->next[i] = i + 1;
	}
	kn->next[kn->capacity] = 0;

	return kn;
}

static void *do_kmalloc(struct kmem_allocator *k, uint size)
{
	acquire(&k->lock);
	if (k->list == NULL && (k->list = alloc_node(size)) == NULL) {
		release(&k->lock);
		return NULL;
	}

	struct kmem_node *kn = k->list;
	int index = kn->next[0];
	kn->next[0] = kn->next[index];
	kn->next[index] = 0;
	kn->nfree--;
	if (kn->nfree == 0) {     // last allocated, this node is full
		k->list = kn->list;   // remove it from allocator's list
		kn->list = NULL;
	}

	void *obj = (char *)kn->objs + (index - 1) * size;
	release(&k->lock);

	return obj;
}

// At present, we only support some speical size to be allocated, such as struct proc, file and so on
void *kmalloc(uint size)
{
	size = (size + 7) & ~7;  // rounds up, align 8
	int index = get_alloc_idx(size);
	if (index < 0)
		panic("kmalloc: size not supported");

	acquire(&alloc_lock);
	if (allocs[index] == NULL) {
		if ((allocs[index] = do_kmalloc(&alloc_min, KMEM_OBJ_MIN_SIZE)) == NULL) {
			release(&alloc_lock);
			return NULL;
		}
		initlock(&allocs[index]->lock, "kmem");
		allocs[index]->list = NULL;
	}
	release(&alloc_lock);

	return do_kmalloc(allocs[index], size);
}

void kfree(void *p)
{
	
	struct kmem_node *kn = (struct kmem_node *)PGROUNDDOWN((uint64)p);
	struct kmem_allocator *k = get_allocator(kn->size);
	int index = ((char *)p - (char *)kn->objs) / kn->size + 1;

	acquire(&k->lock);
	kn->next[index] = kn->next[0];
	kn->next[0] = index;
	kn->nfree++;

	if (kn->nfree == 1) {   // a full node becomes available, join allocator's list
		struct kmem_node **n;
		for (n = &k->list; *n != NULL; n = &(*n)->list);
		*n = kn;
	} else if (kn->nfree == kn->capacity) { // a node is totally empty, free the page it takes up
		struct kmem_node **n;
		for (n = &k->list; *n != kn; n = &(*n)->list);
		*n = kn->list;
		freepage(kn);
	}

	release(&k->lock);
}

void kmview()
{
	acquire(&alloc_lock);
	for (int i = 0; i < NELEM(allocs); i++) {
		if (allocs[i] == NULL)
			continue;
		acquire(&allocs[i]->lock);
		printf("allocator %d:\n", i);
		struct kmem_node *kn;
		int count = 1;
		for (kn = allocs[i]->list; kn != NULL; kn = kn->list) {
			printf("\tNode %d: size=%d, capa=%d, taken=%d\n", count++, kn->size, kn->capacity, kn->capacity - kn->nfree);
		}
		release(&allocs[i]->lock);
	}
	release(&alloc_lock);
}

void kmtest()
{
	printf("\n==================== test 1 ====================\n");

	printf("[kmtest] check point 1, hart %d\n", r_tp());
	kmview();

	struct proc *p0 = kmalloc(sizeof(struct proc));
	struct proc *p1 = kmalloc(sizeof(struct proc));
	struct proc *p2 = kmalloc(sizeof(struct proc));
	struct proc *p3 = kmalloc(sizeof(struct proc));

	struct dirent *ep0 = kmalloc(sizeof(struct dirent));
	struct dirent *ep1 = kmalloc(sizeof(struct dirent));
	struct dirent *ep2 = kmalloc(sizeof(struct dirent));
	struct dirent *ep3 = kmalloc(sizeof(struct dirent));

	struct buf *b0 = kmalloc(sizeof(struct buf));
	struct buf *b1 = kmalloc(sizeof(struct buf));
	struct buf *b2 = kmalloc(sizeof(struct buf));
	struct buf *b3 = kmalloc(sizeof(struct buf));

	struct file *f0 = kmalloc(sizeof(struct file));
	struct file *f1 = kmalloc(sizeof(struct file));
	struct file *f2 = kmalloc(sizeof(struct file));
	struct file *f3 = kmalloc(sizeof(struct file));

	printf("[kmtest] check point 2, hart %d\n", r_tp());
	kmview();

	printf("\n==================== test 2 ====================\n");

	struct proc **pn = (struct proc **) allocpage();
	int num = PGSIZE / sizeof(struct proc *);
	for (int i = 0; i < num; i++) {
		pn[i] = (struct proc *) kmalloc(sizeof(struct proc));
		// printf("no.%d: %p\n", i, pn[i]);
		pn[i]->pid = i;
		pn[i]->lock.locked = i; // first field
		pn[i]->tmask = i;       // last field
	}

	// printf("[kmtest] check point 3, hart %d\n", r_tp());
	// kmview();

	for (int i = 0; i < num; i++) {
		if (pn[i]->pid != i ||
			pn[i]->lock.locked != i ||
			pn[i]->tmask != i)
		{
			printf("something went wrong with struct %d\n", i);
		}
		kfree(pn[i]);
	}
	freepage(pn);

	printf("[kmtest] check point 4, hart %d\n", r_tp());
	kmview();

	kfree(p0);
	kfree(p1);
	kfree(p2);
	kfree(p3);

	kfree(ep0);
	kfree(ep1);
	kfree(ep2);
	kfree(ep3);

	kfree(b0);
	kfree(b1);
	kfree(b2);
	kfree(b3);

	kfree(f0);
	kfree(f1);
	kfree(f2);
	kfree(f3);

	printf("[kmtest] check point 5, hart %d\n", r_tp());
	kmview();
	// char **buf = allocpage();
	// int numbuf = PGSIZE / sizeof(char *);
	// uint64 t1, t2;
	// t1 = r_time();
	// asm volatile("rdtime t0");
	// asm volatile("mv %0, t0" : "=r" (t1) );
	// int i;
	// for (i = 0; i < numbuf; i++) {
	//   if ((buf[i] = allocpage()) == NULL) {
	//     break;
	//   }
	// }
	// for (i--; i >= 0; i--) {
	//   freepage(buf[i]);
	// }
	// asm volatile("rdtime t0");
	// asm volatile("mv %0, t0" : "=r" (t2) );
	// printf("[kmtest] t = %d\n", t2 - t1);
	// freepage(buf);
}
