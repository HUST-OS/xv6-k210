// test implemetation

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "sbi.h"

extern uint64 etext_addr;
extern struct proc *initproc;
void test_kalloc() {
    char *mem = kalloc();
    memset(mem, 0, PGSIZE);
    strncpy(mem, "Hello, xv6-k210", 16);
    printf("[test_kalloc]mem: %s\n", mem);
}

void ptesprint(pagetable_t pagetable, int level){
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      for(int j = 0; j < level-1; j++)
        printf(".. ");
      printf("..%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
    }
    if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      ptesprint((pagetable_t)child, level+1);
    }
  }
}

int vmprint(pagetable_t pagetable){

  printf("page table %p\n", pagetable);
  ptesprint(pagetable, 1);

  return 0;
}

void test_vm(unsigned long hart_id) {
  printf("UART:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", UART0, kvmpa(UART0));
  printf("virto mmio:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", VIRTIO0, kvmpa(VIRTIO0));
  printf("CLINT:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", CLINT, kvmpa(CLINT));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", CLINT_MTIMECMP(hart_id), kvmpa(CLINT_MTIMECMP(hart_id)));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", CLINT_MTIME, kvmpa(CLINT_MTIME));
  printf("PLIC\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC, kvmpa(PLIC));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_PRIORITY, kvmpa(PLIC_PRIORITY));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_PENDING, kvmpa(PLIC_PENDING));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_MENABLE(hart_id), kvmpa(PLIC_MENABLE(hart_id)));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_SENABLE(hart_id), kvmpa(PLIC_SENABLE(hart_id)));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_MPRIORITY(hart_id), kvmpa(PLIC_MPRIORITY(hart_id)));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_SPRIORITY(hart_id), kvmpa(PLIC_SPRIORITY(hart_id)));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_MCLAIM(hart_id), kvmpa(PLIC_MCLAIM(hart_id)));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_SCLAIM(hart_id), kvmpa(PLIC_SCLAIM(hart_id)));
  printf("kernel base:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE, kvmpa(KERNBASE));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE + 0x1000, kvmpa(KERNBASE + 0x1000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE + 0x2000, kvmpa(KERNBASE + 0x2000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE + 0x3000, kvmpa(KERNBASE + 0x3000));
  printf("etext_addr:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", etext_addr, kvmpa(etext_addr));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", etext_addr + 0x1000, kvmpa(etext_addr + 0x1000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", etext_addr + 0x2000, kvmpa(etext_addr + 0x2000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", etext_addr + 0x3000, kvmpa(etext_addr + 0x3000));
  printf("trampoline:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", TRAMPOLINE, kvmpa(TRAMPOLINE));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", TRAMPOLINE + PGSIZE - 1, kvmpa(TRAMPOLINE + PGSIZE - 1));
  printf("[test_vm]create test pagetable\n");
  pagetable_t test_pagetable = uvmcreate();
  printf("test_pagetable: %p\n", test_pagetable);
  char *test_mem = kalloc();
  memset(test_mem, 0, PGSIZE);
  if(mappages(test_pagetable, 0, PGSIZE, (uint64)test_mem, PTE_R | PTE_W | PTE_U | PTE_X) != 0) {
    panic("[test_vm]mappages failed\n");
  }
  printf("[test_vm](walkaddr) va: %p, pa: %p\n", 0, walkaddr(test_pagetable, 0));
  printf("[test_vm](walkaddr) va: %p, pa: %p\n", PGSIZE - 1, walkaddr(test_pagetable, PGSIZE - 1) + (PGSIZE - 1) % PGSIZE);
}
