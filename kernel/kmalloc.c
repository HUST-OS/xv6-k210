/**
 * A small-mem allocator implement inspired by the slab-mechanism of Linux.
 * However, our design is much easier and less considered. We ignore the
 * effect of the hardware-cache by excluding the CPU struct in the kmem_cache.
 */

#ifndef __DEBUG_kmalloc
#undef DEBUG 
#endif 

#include "include/types.h"
#include "include/riscv.h"
#include "include/pm.h"
#include "include/kmalloc.h"
#include "include/spinlock.h"
#include "include/printf.h"
#include "include/debug.h"

#define KMEM_OBJ_MIN_SIZE   ((uint64)32)
#define KMEM_OBJ_MAX_SIZE 	((uint64)4048)
#define KMEM_OBJ_MAX_COUNT  (PGSIZE / KMEM_OBJ_MIN_SIZE)

#define TABLE_END 	255
struct kmem_node {
	// `config` is meant to keep const after kmem_node init 
	struct kmem_node *next;
	struct {
		//uint64 obj_capa;			// the capacity of each object 
		uint64 obj_size;			// the size of each object 
		uint64 obj_addr;			// the start addr of first avail object 
	} config;
	uint8 avail;		// current available obj num 
	uint8 cnt;		// current allocated count of obj 
	// size of `next` is not fixed according to config.obj_size
	uint8 table[KMEM_OBJ_MAX_COUNT];	// linked list table,  
};

// the size of fixed part in kmem_node 
#define KMEM_NODE_FIX \
	(sizeof(struct kmem_node*) + 2 * sizeof(uint64) + 2 * sizeof(int8))

struct kmem_allocator {
	struct spinlock lock;		// the lock to protect this allocator 
	uint obj_size;				// the obj size that allocator allocates 
	struct kmem_node *list;		// point to first kmem_node 
	struct kmem_allocator *next;	// point to next kmem_allocator 
};

// the first allocator to allocate other allocators 
struct kmem_allocator kmem_adam;

#define KMEM_TABLE_SIZE 	17
struct kmem_allocator *kmem_table[KMEM_TABLE_SIZE];
struct spinlock kmem_table_lock;

// hash map func 
#define _hash(n) \
	((n) % KMEM_TABLE_SIZE)

#define ROUNDUP16(n) \
	(((n) + 15) & ~0x0f)

// as kmalloc() use allocpage() and freepage, 
// kmallocinit() should be called at least after kpminit() 
void kmallocinit(void) {
	// init adam 
	initlock(&(kmem_adam.lock), "kmem_adam");
	kmem_adam.list = NULL;
	kmem_adam.next = NULL;
	kmem_adam.obj_size = 
			ROUNDUP16(sizeof(struct kmem_allocator));

	// init kmem_table 
	for (uint8 i = 0; i < KMEM_TABLE_SIZE; i++) {
		kmem_table[i] = NULL;
	}
	int hash = _hash(kmem_adam.obj_size);
	kmem_table[hash] = &kmem_adam;
	initlock(&kmem_table_lock, "kmem_table");

	__debug_info("kmallocinit", "KMEM_NODE_FIX: %d\n", KMEM_NODE_FIX);
	__debug_info("kmalloc", "init\n");
}

// the question comes that whether we should free an allocator? 
// when should we do this? 

// current solution doesn't not free an allocator 

#define _malloc_allocator() \
	((struct kmem_allocator*)kmalloc(sizeof(struct kmem_allocator))) 

// to calculate the capa 
#define _calc_capa(roundup_size) \
	((PGSIZE - ROUNDUP16(KMEM_NODE_FIX)) / ((roundup_size) + 1))

// get the allocator for coming allocation 
// `raw_size` means the size may not be aligned 
static struct kmem_allocator *get_allocator(uint64 raw_size) {
	uint64 roundup_size = ROUNDUP16(raw_size);
	uint64 hash = _hash(roundup_size);

	// search if allocator already in table 
	// if allocator exists, then kem_table_lock wouldn't be acquired 
	for (struct kmem_allocator *tmp = kmem_table[hash]; 
			NULL != tmp; tmp = tmp->next) {
		if (roundup_size == tmp->obj_size) {
			return tmp;
		}
	}

	// enter critical section 
	acquire(&kmem_table_lock);
	// if the previous update have created the allocator 
	// needed here
	if (NULL != kmem_table[hash] && 
			kmem_table[hash]->obj_size == roundup_size) {
		release(&kmem_table_lock);
		return kmem_table[hash];
	}

	// if not found, then create 

	// as sizeof(struct kmem_allocator) is guaranteed an 
	// allocator at init time, `_malloc_allocator()` should 
	// not enter `critical section` when calling `get_allocator()`
	struct kmem_allocator *tmp = _malloc_allocator();
	if (NULL != tmp) {
		initlock(&(tmp->lock), "kmem_alloc");
		tmp->list = NULL;
		tmp->obj_size = roundup_size;
		tmp->next = kmem_table[hash];
		kmem_table[hash] = tmp;
	}

	release(&kmem_table_lock);
	// leave critical section 

	return tmp;
}

void *kmalloc(uint size) {
	// border check for `size`
	if (KMEM_OBJ_MIN_SIZE > size) {
		__debug_warn("kmalloc", "size %d too small, reset to %d\n", size, KMEM_OBJ_MIN_SIZE);
		size = KMEM_OBJ_MIN_SIZE;
	}
	else if (KMEM_OBJ_MAX_SIZE < size) {
		__debug_error("kmalloc", "size %d out of border\n", size);
		return NULL;
	}
	struct kmem_allocator *alloc = get_allocator(size);

	// if failed to alloc 
	if (NULL == alloc) {
		__debug_error("kmalloc", "fail to get allocator\n");
		return NULL;
	}

	// enter critical section `alloc`
	acquire(&(alloc->lock));

	// if no page available 
	if (NULL == alloc->list) {
		struct kmem_node *tmp = (struct kmem_node*)allocpage();
		uint roundup_size = ROUNDUP16(size);
		uint8 capa = _calc_capa(roundup_size);

		tmp->next = NULL;
		tmp->config.obj_size = roundup_size;
		tmp->config.obj_addr = (uint64)tmp + ROUNDUP16(KMEM_NODE_FIX + capa);

		tmp->avail = 0;
		tmp->cnt = 0;
		for (uint8 i = 0; i < capa - 1; i ++) {
			tmp->table[i] = i + 1;
		}
		tmp->table[capa - 1] = TABLE_END;

		alloc->list = tmp;
	}

	// now the allocator should be ready 
	struct kmem_node *node = alloc->list;
	void *ret;		// the address to be returned 
	ret = (void*)(node->config.obj_addr + 
			((uint64)node->avail) * node->config.obj_size);
	// update `avail` and `cnt`
	node->cnt += 1;
	node->avail = node->table[node->avail];

	// if kmem_node is fully allocated 
	if (TABLE_END == node->avail) {
		alloc->list = node->next;
	}

	release(&(alloc->lock));
	// leave critical section `alloc`

	return ret;
}

// `addr` must be an address that's allocated before, pass an unallocated 
// address may cause undetectable troubles. 
void kfree(void *addr) {
	struct kmem_node *node = (struct kmem_node*)PGROUNDDOWN((uint64)addr);
	uint8 avail = ((uint64)addr - node->config.obj_addr) / node->config.obj_size;

	struct kmem_allocator *alloc = get_allocator(node->config.obj_size);

	__debug_info("kfree", "alloc: %p, addr: %p\n", alloc, addr);
	// enter critical section `alloc`
	acquire(&(alloc->lock));

	// if `node` used to be fully allocated, then re-link it to `alloc`
	if (TABLE_END == node->avail) {
		node->next = alloc->list;
		alloc->list = node;
		__debug_info("kfree", "pickup\n");
	}

	// node should be on alloc->list 
	node->table[avail] = node->avail;
	node->avail = avail;
	node->cnt -= 1;

	// if kmem_node has no allocated obj 
	if (0 == node->cnt) {
		__debug_info("kfree", "drop\n");
		struct kmem_node **pprev = &(alloc->list);
		struct kmem_node *tmp = alloc->list;

		while (NULL != tmp && node != tmp) {
			pprev = &(tmp->next);
			tmp = tmp->next;
		}
		if (NULL == tmp) {
			__debug_error("free", "NULL == tmp\n");
			panic("kfree(): linked list broken!\n");
		}

		#ifdef DEBUG 
		// display linked list of kmem_allocator 
		for (struct kmem_node *it = alloc->list; NULL != it; it = it->next) {
			printf("%p -> ", it);
		}
		printf("\n");
		#endif 

		*pprev = tmp->next;
		__debug_info("kfree", "alloc->list = %p\n", alloc->list);
		__debug_info("kfree", "tmp = %p\n", tmp);

		freepage(node);
	}

	release(&(alloc->lock));
	// leave critical section `alloc`
}

#ifdef DEBUG 

// display the content of kmem_allocator 
static void km_view_allocator(struct kmem_allocator *alloc) {
	// enter critical section 
	acquire(&(alloc->lock));

	/*printf(__INFO("kmem_allocator")": size %d, addr: %p\n", */
			/*alloc->obj_size, alloc);*/
	__debug_info("kmem_allocator", "size %d, addr: %p\n", 
			alloc->obj_size, alloc);
	for (struct kmem_node *node = alloc->list; NULL != node; node = node->next) {
		printf(
			"\t"__INFO("kmem_node")":\n"
			"\tconfig.obj_size: %d\n"
			"\tconfig.obj_addr: %p\n"
			"\tavail: %d\n"
			"\tcnt: %d\n", 
			node->config.obj_size, 
			node->config.obj_addr, 
			node->avail, 
			node->cnt
		);
		printf("\t[");
		for (uint8 i = node->avail; TABLE_END != i; i = node->table[i]) {
			printf("%d ", i);
		}
		printf("]\n");
	}

	release(&(alloc->lock));
	// leave critical section 
}

// for debugging 
void kmview(void)
{
	// display all the content in kmem_table

	// enter critical section 
	acquire(&kmem_table_lock);

	for (int i = 0; i < KMEM_TABLE_SIZE; i ++) {
		struct kmem_allocator *alloc = kmem_table[i];

		while (NULL != alloc) {
			km_view_allocator(alloc);
			alloc = alloc->next;
		}
	}

	release(&kmem_table_lock);
	// leave critical section 
}

#include "include/proc.h"
#include "include/fat32.h"
#include "include/buf.h"
#include "include/file.h"

void kmtest(void)
{
	__debug_info("kmemtest", "check point 1, hart %d\n", r_tp());
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

	__debug_info("kmemtest", "check point 2, hart %d\n", r_tp());
	kmview();

	struct proc **pn = (struct proc **) allocpage();
	int num = PGSIZE / sizeof(struct proc *);
	for (int i = 0; i < num; i++) {
		pn[i] = (struct proc *) kmalloc(sizeof(struct proc));
		pn[i]->pid = i;
		pn[i]->lock.locked = i; // first field
		pn[i]->tmask = i;       // last field
	}

	__debug_info("kmemtest", "check point 3, hart %d\n", r_tp());
	kmview();

	for (int i = 0; i < num; i++) {
		if (pn[i]->pid != i ||
			pn[i]->lock.locked != i ||
			pn[i]->tmask != i)
		{
			printf(__ERROR("kmemtest")": something went wrong with struct %d\n", i);
		}
		kfree(pn[i]);
	}
	freepage(pn);

	__debug_info("kmemtest", "check point 4, hart %d\n", r_tp());
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

	__debug_info("kmemtest", "check point 5, hart %d\n", r_tp());
	kmview();

	// Boarder cases 
	void *mem_min = kmalloc(0);
	void *mem_max = kmalloc(PGSIZE);
	void *mem_max_around_border = kmalloc(4040);
	void *mem_min_around_border = kmalloc(30);

	__debug_info("kmemtest", "check point 5, hart %d\n", r_tp());
	kmview();

	kfree(mem_min);
	if (NULL != mem_max) {
		__debug_error("kmemtest", "mem_max wrong alloc\n");
	}
	kfree(mem_min_around_border);
	kfree(mem_max_around_border);

	__debug_info("kmemtest", "check point 6, hart %d\n", r_tp());
	kmview();
}

#endif 
