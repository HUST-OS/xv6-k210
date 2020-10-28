#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "sbi.h"

volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
void
main(unsigned long hartid, unsigned long dtb_pa)
{
  
  // printf("hart %d enter main()...\n", hartid);
  if (hartid == 0) {
    printf("\n");
    printf("xv6-k210 kernel is booting\n");
    printf("\n");

    kinit();         // physical page allocator
    kvminit();       // create kernel page table
    kvminithart();   // turn on paging
    trapinit();      // trap vectors
    trapinithart();  // install kernel trap vector
    timerinit();     // set up timer interrupt handler
    procinit();
    device_init(dtb_pa, hartid);
    // plicinit();      // set up interrupt controller
    // plicinithart();  // ask PLIC for device interrupts
    // binit();         // buffer cache
    // iinit();         // inode cache
    // fileinit();      // file table
    // virtio_disk_init(); // emulated hard disk
    // userinit();      // first user process
    
    test_kalloc();    // test kalloc
    test_vm(hartid);       // test kernel pagetable
    test_proc_init();   // test porc init

    printf("hart 0 init done\n");
    for(int i = 1; i < NCPU; i++) {
      unsigned long mask = 1 << i;
      sbi_send_ipi(&mask);
    }
    __sync_synchronize();
    started = 1;

    scheduler();

  } else
  {
    // hart 1
    // sbi_console_putchar('A');
    // sbi_console_putchar('B');
    // sbi_console_putchar('C');
    // sbi_console_putchar('D');
    // sbi_console_putchar('\n');
  }
  
  
  while (1) {
  }  
}
