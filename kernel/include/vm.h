#ifndef __VM_H 
#define __VM_H 

#include "types.h"
#include "riscv.h"

static inline void permit_usr_mem()
{
  #ifndef QEMU
  w_sstatus(r_sstatus() & ~SSTATUS_PUM);
  #else
  w_sstatus(r_sstatus() | SSTATUS_SUM);
  #endif
}

static inline void protect_usr_mem()
{
  #ifndef QEMU
  w_sstatus(r_sstatus() | SSTATUS_PUM);
  #else
  w_sstatus(r_sstatus() & ~SSTATUS_SUM);
  #endif
}

void            kvminit(void);
void            kvminithart(void);
void            kvmmap(uint64 va, uint64 pa, uint64 sz, int perm);
pagetable_t     kvmcreate(void);
void            kvmfree(pagetable_t kpt, int stack_free);

void            uvminit(pagetable_t, uchar *, uint);
pagetable_t     uvmcreate(void);
int             uvmcopy(pagetable_t, pagetable_t, pagetable_t, uint64);
int             uvmcopy_cow(pagetable_t old, pagetable_t new, uint64 start, uint64 end);
uint64          uvmalloc(pagetable_t, uint64, uint64);
uint64          uvmdealloc(pagetable_t, uint64, uint64);
void            uvmclear(pagetable_t, uint64);
void            uvmfree(pagetable_t pt, uint64 sz);

uint64          walkaddr(pagetable_t, uint64);
uint64          kwalkaddr(pagetable_t pagetable, uint64 va);
uint64          kvmpa(uint64);

int             mappages(pagetable_t pt, uint64 va, uint64 size, uint64 pa, int perm, int usr);
void            unmappages(pagetable_t pt, uint64 va, uint64 npages, int do_free, int usr);

int             copyout(pagetable_t, uint64, char *, uint64);
int             copyin(pagetable_t, char *, uint64, uint64);
int             copyinstr(pagetable_t, char *, uint64, uint64);
int             copyout2(uint64 dstva, char *src, uint64 len);
int             copyin2(char *dst, uint64 srcva, uint64 len);
int             copyinstr2(char *dst, uint64 srcva, uint64 max);
void            vmprint(pagetable_t pagetable);

int             handle_page_fault(int type, uint64 badaddr);

#endif 
