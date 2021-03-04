// Copyright (c) 2006-2019 Frans Kaashoek, Robert Morris, Russ Cox,
//                         Massachusetts Institute of Technology

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/sbi.h"
#include "include/sdcard.h"
#include "include/fpioa.h"

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
    printf("hart %d enter main()...\n", hartid);
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    timerinit();     // set up timer interrupt handler
    procinit();

	// init file system 
    #ifndef QEMU
    fpioa_pin_init();

	#ifdef QEMU 
	plicinit();
	plicinithart();
	#endif 

    disk_init();
	test_sdcard();
	while (1);

    binit();         // buffer cache
    fileinit();      // file table

    userinit();      // first user process

    printf("hart 0 init done\n");
    for(int i = 1; i < NCPU; i++) {
      unsigned long mask = 1 << i;
      sbi_send_ipi(&mask);
    }
    __sync_synchronize();
    started = 1;
  } else
  {
    // hart 1
    while (started == 0)
      ;
    __sync_synchronize();
    printf("hart %d enter main()...\n", hartid);
    kvminithart();
    trapinithart();
    timerinit();     // set up timer interrupt handler
    #ifndef QEMU
    device_init(dtb_pa, hartid);
    #else
    //plicinithart();  // ask PLIC for device interrupts
    #endif
    printf("hart 1 init done\n");
  }
  scheduler();
}
