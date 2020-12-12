// test implemetation


#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/defs.h"
#include "include/sbi.h"
#include "include/sdcard.h"

extern char etext[];
extern struct proc *initproc;
void test_kalloc() {
    char *mem = kalloc();
    memset(mem, 0, PGSIZE);
    strncpy(mem, "Hello, xv6-k210", 16);
    printf("[test_kalloc]mem: %s\n", mem);
    kfree(mem);
}

void test_vm(unsigned long hart_id) {
  #ifndef QEMU
  printf("[test_vm]UARTHS:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", UARTHS, kvmpa(UARTHS));
  #else
  printf("virto mmio:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", VIRTIO0, kvmpa(VIRTIO0));
  #endif
  printf("[test_vm]CLINT:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", CLINT, kvmpa(CLINT));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", CLINT_MTIMECMP(hart_id), kvmpa(CLINT_MTIMECMP(hart_id)));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", CLINT_MTIME, kvmpa(CLINT_MTIME));
  printf("[test_vm]PLIC\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC, kvmpa(PLIC));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_PRIORITY, kvmpa(PLIC_PRIORITY));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_PENDING, kvmpa(PLIC_PENDING));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_MENABLE(hart_id), kvmpa(PLIC_MENABLE(hart_id)));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_SENABLE(hart_id), kvmpa(PLIC_SENABLE(hart_id)));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_MPRIORITY(hart_id), kvmpa(PLIC_MPRIORITY(hart_id)));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_SPRIORITY(hart_id), kvmpa(PLIC_SPRIORITY(hart_id)));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_MCLAIM(hart_id), kvmpa(PLIC_MCLAIM(hart_id)));
//   printf("[test_vm](kvmpa) va: %p, pa: %p\n", PLIC_SCLAIM(hart_id), kvmpa(PLIC_SCLAIM(hart_id)));
  
  printf("[test_vm]rustsbi:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", RUSTSBI_BASE, kvmpa(RUSTSBI_BASE));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", RUSTSBI_BASE + 0x1000, kvmpa(RUSTSBI_BASE + 0x1000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", RUSTSBI_BASE + 0x2000, kvmpa(RUSTSBI_BASE + 0x2000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", RUSTSBI_BASE + 0x3000, kvmpa(RUSTSBI_BASE + 0x3000));
  printf("[test_vm]kernel base:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE, kvmpa(KERNBASE));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE + 0x1000, kvmpa(KERNBASE + 0x1000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE + 0x2000, kvmpa(KERNBASE + 0x2000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", KERNBASE + 0x3000, kvmpa(KERNBASE + 0x3000));
  printf("[test_vm]etext:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", (uint64)etext, kvmpa((uint64)etext));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", (uint64)etext + 0x1000, kvmpa((uint64)etext + 0x1000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", (uint64)etext + 0x2000, kvmpa((uint64)etext + 0x2000));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", (uint64)etext + 0x3000, kvmpa((uint64)etext + 0x3000));
  printf("[test_vm]trampoline:\n");
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", TRAMPOLINE, kvmpa(TRAMPOLINE));
  printf("[test_vm](kvmpa) va: %p, pa: %p\n", TRAMPOLINE + PGSIZE - 1, kvmpa(TRAMPOLINE + PGSIZE - 1));
  printf("[test_vm]create test pagetable\n");
  pagetable_t test_pagetable = uvmcreate();
  printf("[test_vm]test_pagetable: %p\n", test_pagetable);
  char *test_mem = kalloc();
  memset(test_mem, 0, PGSIZE);
  if(mappages(test_pagetable, 0, PGSIZE, (uint64)test_mem, PTE_R | PTE_W | PTE_U | PTE_X) != 0) {
    panic("[test_vm]mappages failed\n");
  }
  printf("[test_vm](walkaddr) va: %p, pa: %p\n", 0, walkaddr(test_pagetable, 0));
  printf("[test_vm](walkaddr) va: %p, pa: %p\n", PGSIZE - 1, walkaddr(test_pagetable, PGSIZE - 1) + (PGSIZE - 1) % PGSIZE);
}

#ifndef QEMU
void test_sdcard() {
  uint8 *buffer = kalloc();
  uint8 *pre_buffer = kalloc();
  memset(buffer, 0, sizeof(buffer));
  if(sd_read_sector(pre_buffer, 0, sizeof(pre_buffer))) {
      printf("[test_sdcard]SD card read sector err\n");
  } else {
      printf("[test_sdcard]SD card read sector succeed\n");
  }
  printf("[test_sdcard]Buffer: %s\n", buffer);
  memmove(buffer, "Hello,sdcard", sizeof("Hello,sdcard"));
  printf("[test_sdcard]Buffer: %s\n", buffer);
  if(sd_write_sector(buffer, 0, sizeof(buffer))) {
      printf("[test_sdcard]SD card write sector err\n");
  } else {
      printf("[test_sdcard]SD card write sector succeed\n");
  }
  memset(buffer, 0, sizeof(buffer));
  if(sd_read_sector(buffer, 0, sizeof(buffer))) {
      printf("[test_sdcard]SD card read sector err\n");
  } else {
      printf("[test_sdcard]SD card read sector succeed\n");
  }
  printf("[test_sdcard]Buffer: %s\n", buffer);
  if(sd_write_sector(pre_buffer, 0, sizeof(pre_buffer))) {
      printf("[test_sdcard]SD card recover err\n");
  } else {
      printf("[test_sdcard]SD card recover succeed\n");
  }
  kfree(buffer);
  kfree(pre_buffer);
}
#endif