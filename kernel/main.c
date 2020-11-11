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
// start() jumps here in supervisor mode on all CPUs.
void
main(unsigned long hartid, unsigned long dtb_pa)
{
  inithartid(hartid);
  
  if (hartid == 0) {
    printfinit();   // init a lock for printf 
    printf("\n");
    printf("xv6-k210 kernel is booting\n");
    printf("\n");
    printf("hart %d enter main()...\n", hartid);
    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    timerinit();     // set up timer interrupt handler
    procinit();
    device_init(dtb_pa, hartid);
    fpioa_pin_init();
    sdcard_init();
    //plicinit();      // set up interrupt controller
    //plicinithart();  // ask PLIC for device interrupts
    binit();         // buffer cache
    iinit();         // inode cache
    fileinit();      // file table
    //virtio_disk_init(); // emulated hard disk
    userinit();      // first user process
    
    // test_kalloc();    // test kalloc
    // test_vm(hartid);       // test kernel pagetable
    // test_proc_init();   // test porc init
    // test_sdcard();

    printf("hart 0 init done\n");
    // scheduler();
    // while (1) {
    // }  
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
    printfinit();   // init a lock for printf 
    // printf("hart %d enter main()...\n", hartid);
    kvminithart();
    trapinithart();
    device_init(dtb_pa, hartid);
    // printf("hart 1 init done\n");
  }
  scheduler();
  while (1) {
  }  
}
