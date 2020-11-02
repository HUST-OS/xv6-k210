
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/sbi.h"
#include "include/sdcard.h"
#include "include/fpioa.h"

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
    fpioa_set_function(27, FUNC_SPI0_SCLK);
    fpioa_set_function(28, FUNC_SPI0_D0);
    fpioa_set_function(26, FUNC_SPI0_D1);
	  fpioa_set_function(32, FUNC_GPIOHS7);
    fpioa_set_function(29, FUNC_SPI0_SS3);
    uint8 cardinfo = sd_init();
    if(cardinfo) {
      panic("sd card init error\n");
    } else
    {
      printf("sdcard init: %d\n", cardinfo);
    }
    
    uint8 buffer[100];
    memset(buffer, 0, sizeof(buffer));
    if(sd_read_sector(buffer, 0, 10)) {
        printf("SD card read sector err\n");
    } else {
        printf("SD card read sector succeed\n");
    }
    memmove(buffer, "Hello,World", sizeof("Hello,World"));
    printf("Buffer: %s\n", buffer);
    if(sd_write_sector(buffer, 0, 10)) {
        printf("SD card write sector err\n");
    } else {
        printf("SD card write sector succeed\n");
    }
    memset(buffer, 0, sizeof(buffer));
    if(sd_read_sector(buffer, 0, 10)) {
        printf("SD card read sector err\n");
    } else {
        printf("SD card read sector succeed\n");
    }
    printf("Buffer: %s\n", buffer);
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

    // scheduler();

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
