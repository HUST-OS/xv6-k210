// Copyright (c) 2006-2019 Frans Kaashoek, Robert Morris, Russ Cox,
//                         Massachusetts Institute of Technology

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/sbi.h"
#include "include/console.h"
#include "include/printf.h"
#include "include/timer.h"
#include "include/trap.h"
#include "include/proc.h"
#include "include/plic.h"
#include "include/pm.h"
#include "include/vm.h"
#include "include/kmalloc.h"
#include "include/disk.h"
#include "include/buf.h"
#include "include/debug.h"
#ifndef QEMU
#include "include/fpioa.h"
#include "include/dmac.h"
#endif

static inline void inithartid(unsigned long hartid) {
  asm volatile("mv tp, %0" : : "r" (hartid & 0x1));
}

volatile static int started = 0;

void
main(unsigned long hartid, unsigned long dtb_pa)
{
  inithartid(hartid);
  
  if (hartid == 0) {
    consoleinit();
    printfinit();   // init a lock for printf 
    print_logo();
    #ifdef DEBUG
    printf("hart %d enter main()...\n", hartid);
    #endif
    kpminit();       // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    kmallocinit();   // small physical memory allocator
    // timerinit();     // init a lock for timer
    trapinithart();  // install kernel trap vector, including interrupt handler
    procinit();
    plicinit();
    plicinithart();
    #ifndef QEMU
    fpioa_pin_init();
    dmac_init();
    #endif 
    disk_init();
    binit();         // buffer cache
    fileinit();      // file table
    userinit();      // first user process
    printf("hart 0 init done\n");

	__debug_kmtest();
    
	// is ipi necessary here?
	for(int i = 1; i < NCPU; i++) {
	  unsigned long mask = 1 << i;
	  sbi_send_ipi(&mask);
	}
    __sync_synchronize();
    started = 1;
  }
  else
  {
    // hart 1
    while (started == 0)
      ;
    __sync_synchronize();
    #ifdef DEBUG
    printf("hart %d enter main()...\n", hartid);
    #endif
    kvminithart();
    trapinithart();
    plicinithart();  // ask PLIC for device interrupts
    printf("hart 1 init done\n");

	__debug_kmtest();
  }
  scheduler();
}
