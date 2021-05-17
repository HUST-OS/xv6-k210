#ifndef __DEBUG_vm
#undef DEBUG
#endif

#include "include/param.h"
#include "include/types.h"
#include "include/memlayout.h"
#include "include/elf.h"
#include "include/riscv.h"
#include "include/vm.h"
#include "include/pm.h"
#include "include/usrmm.h"
#include "include/proc.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/debug.h"

#define MAX_PAGES_NUM 0x600

#define PTE_COW PTE_RSW1  // Use it to mark a COW page

/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;
static struct spinlock page_ref_lock;
static uint8 page_ref_table[MAX_PAGES_NUM];  // user pages ref, for COW fork mechanism

extern char etext[];  // kernel.ld sets this to end of kernel code.
extern char trampoline[]; // trampoline.S
extern char kernel_end[]; // first address after kernel.
/*
 * create a direct-map page table for the kernel.
 */
void
kvminit()
{
  if (idlepages() > MAX_PAGES_NUM)
    panic("kvminit: page_ref_table[] not enough");

  initlock(&page_ref_lock, "page_ref_lock");
	memset(page_ref_table, 0, sizeof(page_ref_table));

  kernel_pagetable = (pagetable_t) allocpage();
  // printf("kernel_pagetable: %p\n", kernel_pagetable);

  memset(kernel_pagetable, 0, PGSIZE);

  // uart registers
  kvmmap(UART_V, UART, PGSIZE, PTE_R | PTE_W);
  
  #ifdef QEMU
  // virtio mmio disk interface
  kvmmap(VIRTIO0_V, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  #endif
  // CLINT
  kvmmap(CLINT_V, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC_V, PLIC, 0x4000, PTE_R | PTE_W);
  kvmmap(PLIC_V + 0x200000, PLIC + 0x200000, 0x4000, PTE_R | PTE_W);

  #ifndef QEMU
  // GPIOHS
  kvmmap(GPIOHS_V, GPIOHS, 0x1000, PTE_R | PTE_W);

  // DMAC
  kvmmap(DMAC_V, DMAC, 0x1000, PTE_R | PTE_W);

  // GPIO
  // kvmmap(GPIO_V, GPIO, 0x1000, PTE_R | PTE_W);

  // SPI_SLAVE
  kvmmap(SPI_SLAVE_V, SPI_SLAVE, 0x1000, PTE_R | PTE_W);

  // FPIOA
  kvmmap(FPIOA_V, FPIOA, 0x1000, PTE_R | PTE_W);

  // SPI0
  kvmmap(SPI0_V, SPI0, 0x1000, PTE_R | PTE_W);

  // SPI1
  kvmmap(SPI1_V, SPI1, 0x1000, PTE_R | PTE_W);

  // SPI2
  kvmmap(SPI2_V, SPI2, 0x1000, PTE_R | PTE_W);

  // SYSCTL
  kvmmap(SYSCTL_V, SYSCTL, 0x1000, PTE_R | PTE_W);
  
  #endif
  
  // map rustsbi
  // kvmmap(RUSTSBI_BASE, RUSTSBI_BASE, KERNBASE - RUSTSBI_BASE, PTE_R | PTE_X);
  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);
  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);
  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  #ifdef DEBUG
  printf("kvminit\n");
  #endif
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void
kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
  protect_usr_mem();
  #ifdef DEBUG
  printf("kvminithart\n");
  #endif
}

static int __hash_page_idx(uint64 pa)
{
	if (pa % PGSIZE || pa < PGROUNDUP((uint64)kernel_end) || pa >= PHYSTOP)
		panic("__hash_page");
	return (pa - PGROUNDUP((uint64)kernel_end)) >> PGSHIFT;
}

// Register a page for user, init it for later dup
static inline void pagereg(uint64 pa, uint8 init)
{
  acquire(&page_ref_lock);
	page_ref_table[__hash_page_idx(pa)] = init;
  release(&page_ref_lock);
}

// This is used in page fault handler. If two process write the same page
// in the same time, let the process who gets the lock monopolize the page.
static int monopolizepage(uint64 pa)
{
  acquire(&page_ref_lock);
	int idx = __hash_page_idx(pa);
	if (page_ref_table[idx] == 1) {
  	release(&page_ref_lock);
		return 1;
	}
	page_ref_table[idx]--; // should hold the lock until copying done
	return 0;
}

static inline void pagecopydone(void)
{
	release(&page_ref_lock);
}

static inline int pagedup(uint64 pa)
{
  acquire(&page_ref_lock);
  int ref = ++page_ref_table[__hash_page_idx(pa)];
  release(&page_ref_lock);
  // __debug_info("pagedup", "page=%p, ref=%d\n", pa, ref);
	return ref;
}

static inline int pageput(uint64 pa)
{
  acquire(&page_ref_lock);
  int ref = --page_ref_table[__hash_page_idx(pa)];
  release(&page_ref_lock);
  // __debug_info("pageput", "page=%p, ref=%d\n", pa, ref);
	return ref;
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  
  if(va >= MAXVA)
    panic("walk");

  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_V) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pde_t*)allocpage()) == NULL)
        return NULL;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return NULL;

  pte = walk(pagetable, va, 0);
  if(pte == 0)
    return NULL;
  if((*pte & PTE_V) == 0)
    return NULL;
  if((*pte & PTE_U) == 0)
    return NULL;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void
kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
uint64
kvmpa(uint64 va)
{
  return kwalkaddr(kernel_pagetable, va);
}

uint64
kwalkaddr(pagetable_t kpt, uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
  
  pte = walk(kpt, va, 0);
  if(pte == 0)
    panic("kwalkaddr1");
  if((*pte & PTE_V) == 0)
    panic("kwalkaddr2");
	if((*pte & PTE_U))
    panic("kwalkaddr3");
  pa = PTE2PA(*pte);
  return pa+off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);

	int usr = perm & PTE_U;
  for(;;){
    if((pte = walk(pagetable, a, 1)) == NULL)
      return -1;
    if(*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
		if (usr)
			pagedup(PGROUNDDOWN(pa));  // increase ref to parent's page
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
/**
 * @param     flag  see include/vm.h
 */
void
unmappages(pagetable_t pagetable, uint64 va, uint64 npages, int flag)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("unmappages: not aligned");

	int do_free = flag & VM_FREE;
	int usr = flag & VM_USER;
	int allowhole = flag & VM_HOLE;

	uint64 end = va + npages * PGSIZE;
  for (a = va; a < end; a += PGSIZE) {
    if ((pte = walk(pagetable, a, 0)) == NULL || !(*pte & PTE_V)) {
			if (allowhole) continue;  // it might be a lazy-allocated page which is not really allocated, but how could we verify that?
			panic("unmappages: unmapped pte");
		}
    if(PTE_FLAGS(*pte) == PTE_V)
      panic("unmappages: not a leaf");
		uint64 pa = PTE2PA(*pte);
		if (do_free && (!usr || pageput(pa) == 0)) {
			freepage((void*)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
/**
 * With one page shared by kernel and user proc,
 * this func is unused, use kvmcreate instead.
 */
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t) allocpage();
  if(pagetable == NULL)
    return NULL;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void
uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = allocpage();
  memset(mem, 0, PGSIZE);
	pagereg((uint64)mem, 0);	// mappages will increase it
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
/**
 * @param   start   user virtual addr that start to be allocated
 * @param   end     the ending user virtual addr of the alloction
 * @param   perm    permission flags, PTE_W|R|X (PTE_U is a default flag)
 * @return          param end if successful else 0
 */
uint64
uvmalloc(pagetable_t pagetable, uint64 start, uint64 end, int perm)
{
  char *mem;
  uint64 a;

  if (end > MAXUVA)
    return 0;
  if(end < start)
    return start;

  for(a = PGROUNDUP(start); a < end; a += PGSIZE){
    mem = allocpage();
    if (mem == NULL) {
      uvmdealloc(pagetable, start, a, NONE);
      return 0;
    }
    memset(mem, 0, PGSIZE);
		pagereg((uint64)mem, 0);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, perm|PTE_U) != 0) {
      freepage(mem);
      uvmdealloc(pagetable, start, a, NONE);
      return 0;
    }
  }
  return end;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
/**
 * This function only delete physical pages but not pagetable
 * 
 * @param   start   user virtual addr that start to be deleted (low addr)
 * @param   end     the ending user virtual addr of the deletion
 * @param   segt    type of the segment where we perform the deletion
 * @return          param start if successful else 0
 */
uint64
uvmdealloc(pagetable_t pagetable, uint64 start, uint64 end, enum segtype segt)
{
  if(start >= end)
    return end;

	int heapflag = segt == HEAP ? VM_HOLE : 0;
  if (PGROUNDUP(start) < PGROUNDUP(end)) {
    int npages = (PGROUNDUP(end) - PGROUNDUP(start)) / PGSIZE;
    unmappages(pagetable, PGROUNDUP(start), npages, VM_FREE|VM_USER|heapflag);
  }

  return start;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void
freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      freewalk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_V){
      panic("freewalk: leaf");
    }
  }
  freepage((void*)pagetable);
}

/**
 * Use usrmm to free all segments then call this to free
 * the user part of the pagetable. The kernel part is handled
 * by kvmfree()
 */
void
uvmfree(pagetable_t pagetable)
{
  pte_t pte;
  for (int i = 0; i < PX(2, MAXUVA); i++) {
    pte = pagetable[i];
    if (pte & PTE_V) {
      freewalk((pagetable_t) PTE2PA(pte));
      pagetable[i] = 0;
    }
  }
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
// int
// uvmcopy(pagetable_t old, pagetable_t new, pagetable_t knew, uint64 sz)
// {
//   pte_t *pte;
//   uint64 pa, i = 0, ki = 0;
//   uint flags;
//   char *mem;

//   while (i < sz){
//     if((pte = walk(old, i, 0)) == NULL)
//       panic("uvmcopy: pte should exist");
//     if((*pte & PTE_V) == 0)
//       panic("uvmcopy: page not present");
//     pa = PTE2PA(*pte);
//     flags = PTE_FLAGS(*pte);
//     if((mem = allocpage()) == NULL)
//       goto err;
//     memmove(mem, (char*)pa, PGSIZE);
// 		pagereg((uint64)mem, 0);
//     if(mappages(new, i, PGSIZE, (uint64)mem, flags, 1) != 0) {
//       freepage(mem);
//       goto err;
//     }
//     i += PGSIZE;
//     if(mappages(knew, ki, PGSIZE, (uint64)mem, flags/* & ~PTE_U*/, 0) != 0){
//       goto err;
//     }
//     ki += PGSIZE;
//   }
//   return 0;

//  err:
//   unmappages(knew, 0, ki / PGSIZE, 0, 0);
//   unmappages(new, 0, i / PGSIZE, 1, 1);
//   return -1;
// }

/**
 * @param     segt  segment type, the kernel won't panic 
 *                  where pte == NULL or pte invalid if segt is HEAP
 */
int uvmcopy_cow(pagetable_t old, pagetable_t new, uint64 start, uint64 end,  enum segtype segt)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  int heapflag = segt == HEAP ? VM_HOLE : 0;
  for (i = start; i < end; i += PGSIZE) {
	  if((pte = walk(old, i, 0)) == NULL || !(*pte & PTE_V)) {
			if (heapflag) continue;  // a lazy-allocation page, just ignore it, page-fault handler will handle it
			panic("uvmcopy_cow: funny pte");	
	  }
		pa = PTE2PA(*pte);
		if (*pte & PTE_W) {
			*pte = (*pte|PTE_COW) & ~PTE_W;  // cancel 'W', and mark COW
		}
    flags = PTE_FLAGS(*pte);
    if(mappages(new, i, PGSIZE, pa, flags) != 0) {
      goto err;
    }
  }

	// Copy successfully, now cancel 'W' for parent.
	// In fact, it's OK to mark COW for parent's pagetable at previous step
	// for (i = start; i < end; i += PGSIZE) {
	// 	if ((pte = walk(old, i, 0)) == NULL || !(*pte & PTE_V))
	// 		continue;
	// 	if (PTE_FLAGS(*pte) & PTE_W) {
	// 		*pte &= ~PTE_W;
	// 		*pte |= PTE_COW;
	// 	}
	// }
	// Should we perform an sfence.vma?
	sfence_vma();
  return 0;

 err:
  unmappages(new, start, i / PGSIZE, VM_FREE|VM_USER|heapflag);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
/**
 * With usrmm, this func may be abandoned.
 */
void
uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = walk(pagetable, va, 0);
  if(pte == NULL)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

/**
 * Initialize a kernel pagetable for each process with a kernel stack
 */
pagetable_t
kvmcreate()
{
  pagetable_t pagetable;
  char *pstack;
  if ((pagetable = allocpage()) == NULL || (pstack = allocpage()) == NULL) {
		if (pagetable)
			freepage(pagetable);
    return NULL;
	}

	memmove(pagetable, kernel_pagetable, PGSIZE);
	if (mappages(pagetable, VKSTACK, PGSIZE, (uint64)pstack, PTE_R | PTE_W) != 0) {
		pte_t pte = pagetable[PX(2, VKSTACK)];
		if (pte & PTE_V) {
			freewalk((pagetable_t) PTE2PA(pte));
		}
		freepage(pstack);
		freepage(pagetable);
		return NULL;
	}
  return pagetable;
}

// Should be called after uvmfree if the pagetable contains user spase!
// And after trapframe unmapped, otherwise freewalk will panic.
void
kvmfree(pagetable_t pagetable, int stack_free)
{
  if (stack_free) {
    unmappages(pagetable, VKSTACK, 1, VM_FREE);
    pte_t pte = pagetable[PX(2, VKSTACK)];
    if (pte & PTE_V) {
      freewalk((pagetable_t) PTE2PA(pte));
    }
  }
  freepage(pagetable);
}

/**
 * Safe memmove applied when copying memory between kernel and user.
 * 
 * With copy-on-write and lazy-allocation, when kernal calls to copyin(out|instr)2,
 * a page fault may happen.
 * 
 * When user causes a page fault, the hart will trap via usertrap(). If the page
 * fault can not be handled (when lacking of memory or the address is illegal), we
 * can just kill the process by exit() without any bad effect on kernel.
 * 
 * But if it is the kernel that causes the page fault, things might be a little bit
 * different, since some locks are likely to be acquired. We must figure out how to
 * break the memory moving operation and return a failure, mark the process killed
 * but not directly call exit(). Then let kernel return from syscall() and make the
 * process exit() in usertrap().
 */

// back to this point when fail to handle a page fault
static uint64 save_point = 0;
static uint8 safe_state[NCPU];

// a page fault failed to handle, kerneltrap() called for help
uint64 kern_pgfault_escape()
{
	safe_state[r_tp()] = 0; // set unsafe for tht current hart
	return save_point;
}

static void set_safe_state(void)
{
	safe_state[r_tp()] = 1;
}

static int get_safe_state(void)
{
	return safe_state[r_tp()];
}

static int safememmove(void *dst, const void *src, uint len)
{
	char *dst_s = (char *)dst;
	char *src_s = (char *)src;
	int ret = 0;

	set_safe_state();

	// kerneltrap() in trap.c can look up for this safe escape.
	uint64 safe_pc;
	asm volatile("auipc %0, 0x0" : "=r" (safe_pc));
	if (!save_point) {
		__debug_info("safememmove", "save point=%p\n", safe_pc);
		save_point = safe_pc;
	}

	if (get_safe_state() == 1) {
		permit_usr_mem();
		while (len--) {         // There is no overlap between user and kernel space
			*dst_s++ = *src_s++;  // this is possible to raise a page fault
		}
		ret = 1;
	}

	ret--;
	protect_usr_mem();

	if (ret) {
		__debug_error("safememmove", "fail to move\n");
	}
	return ret;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == NULL)
      return -1;
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

int
copyout2(uint64 dstva, char *src, uint64 len)
{
  uint64 sz = myproc()->sz;
  if (dstva + len > sz || dstva >= sz) {
    return -1;
  }
  return safememmove((void *)dstva, src, len);
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == NULL)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}

int
copyin2(char *dst, uint64 srcva, uint64 len)
{
  uint64 sz = myproc()->sz;
  if (srcva + len > sz || srcva >= sz) {
    return -1;
  }
  return safememmove(dst, (void *)srcva, len);
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == NULL)
      return -1;
    n = PGSIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}

int
copyinstr2(char *dst, uint64 srcva, uint64 max)
{
	uint64 sz = myproc()->sz;
	if (srcva >= sz)
		return -1;
  
	// max is given by kernel, we should also check the max of user
	uint64 umax = sz - srcva;
	max = (max <= umax) ? max : umax;
	if (copyin2(dst, srcva, max) < 0) {
		return -1;
	}

  char *old = dst;
  int got_null = 0;
	while (max--) {
		if (*dst == '\0') {
			got_null = 1;
			break;
		}
		dst++;
	}
  if(got_null){
    return dst - old;
  } else {
    return -1;
  }
}

void vmprint(pagetable_t pagetable)
{
  const int capacity = 512;
  printf("page table %p\n", pagetable);
  for (pte_t *pte = (pte_t *) pagetable; pte < pagetable + capacity; pte++) {
    if (*pte & PTE_V)
    {
      pagetable_t pt2 = (pagetable_t) PTE2PA(*pte); 
      printf("..%d: pte %p pa %p\n", pte - pagetable, *pte, pt2);

      for (pte_t *pte2 = (pte_t *) pt2; pte2 < pt2 + capacity; pte2++) {
        if (*pte2 & PTE_V)
        {
          pagetable_t pt3 = (pagetable_t) PTE2PA(*pte2);
          printf(".. ..%d: pte %p pa %p\n", pte2 - pt2, *pte2, pt3);

          for (pte_t *pte3 = (pte_t *) pt3; pte3 < pt3 + capacity; pte3++)
            if (*pte3 & PTE_V)
              printf(".. .. ..%d: pte %p pa %p\n", pte3 - pt3, *pte3, PTE2PA(*pte3));
        }
      }
    }
  }
  return;
}

static int handle_store_page_fault_cow(pte_t *ptep)
{
	pte_t pte = *ptep;
	uint64 pa = PTE2PA(pte);

	if (monopolizepage(pa)) {    // Only this process holds this page.
		pte |= PTE_W;
	} else {
		char *copy = (char *)allocpage();
		if (copy == NULL) {
			pagecopydone();
			return -1;
		}
		memmove(copy, (char *)pa, PGSIZE);
		pagecopydone();
		pagereg((uint64)copy, 1);
		pte = PA2PTE(copy) | PTE_FLAGS(pte) | PTE_W;
	}

	pte &= ~PTE_COW;	// cancel COW mark
	*ptep = pte;
	sfence_vma();

	return 0;
}

static int handle_page_fault_lazy(uint64 badaddr)
{
	// __debug_info("handle_page_fault_lazy", "badaddr=%p\n", badaddr);
	struct proc *p = myproc();

	uint64 pa = PGROUNDDOWN(badaddr);  // in uvmalloc(), oldsz will round up
	if (uvmalloc(p->pagetable, pa, pa + PGSIZE, PTE_W|PTE_R|PTE_X) == 0)
  	return -1;

	sfence_vma();
	return 0;
}

/**
 * @param   type      load-0 | store-1 | access-2
 * @param   badaddr   the stval
 */
int handle_page_fault(int kind, uint64 badaddr)
{
	struct proc *p = myproc();
	// In later implement, we will handle badaddr at heap, mmap zoom and wherever a COW page
	// int section; // Get from new module that is being written
	enum segtype type = typeofseg(p->segment, badaddr);
	if (type == NONE) {
		return -1;
	}
	else if (type == HEAP) {     // a lazy-alloction
		return handle_page_fault_lazy(badaddr);
	}

	pte_t *pte = walk(p->pagetable, badaddr, 0);
	// __debug_info("handle_store_page_fault", 
	// 			"\nbadaddr=%p, pte=%p, *pte=%p\n", badaddr, pte, pte == NULL ? 0 : *pte);
	if (kind == 1 && (*pte & PTE_COW)) { // mapped and store-type, might be a COW fault
		return handle_store_page_fault_cow(pte);
	}

	return -1;
}
