#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sbi.h"
#include "include/plic.h"
#include "include/trap.h"
#include "include/syscall.h"
#include "include/printf.h"
#include "include/console.h"
#include "include/timer.h"
#include "include/disk.h"
#include "include/vm.h"

#define CAUSE_INT_SOFT      0x8000000000000001L
#define CAUSE_INT_TIMER     0x8000000000000005L
#define CAUSE_INT_EXT       0x8000000000000009L

#define CAUSE_EXC_SYS       0x8
#ifdef QEMU
#define CAUSE_EXC_STRPG     0xf
#else
#define CAUSE_EXC_STRPG     0x7
#endif

extern char trampoline[], uservec[], userret[];
extern void kernelvec(); // in kernelvec.S, calls kerneltrap().
int handle_intr(uint64 scause);
int handle_excp(uint64 scause);

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
  w_sstatus(r_sstatus() | SSTATUS_SIE);
  // enable supervisor-mode timer interrupts.
  w_sie(r_sie() | SIE_SEIE | SIE_SSIE | SIE_STIE);
  set_next_timeout();
  #ifdef DEBUG
  printf("trapinithart\n");
  #endif
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void
usertrap(void)
{
  // printf("run in usertrap\n");

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  uint64 cause = r_scause();
  if(cause == CAUSE_EXC_SYS){
    // system call
    if(p->killed)
      exit(-1);
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;
    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();
    syscall();
  } 
  else if (handle_intr(cause) >= 0) {
    // ok
  } 
  else if (handle_excp(cause) < 0) {
		printf("\nusertrap(): unexpected scause %p pid=%d %s\n", cause, p->pid, p->name);
		printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
		// trapframedump(p->trapframe);
		p->killed = 1;
  }

  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(cause == CAUSE_INT_TIMER)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  // printf("[usertrapret]p->pagetable: %p\n", p->pagetable);
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap() {
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if (handle_intr(scause) >= 0) {
		// ok
	}
	else if (handle_excp(scause) < 0) {
    printf("\nscause %p\n", scause);
    printf("sepc=%p stval=%p hart=%d\n", r_sepc(), r_stval(), r_tp());
    struct proc *p = myproc();
    if (p != NULL) {
      printf("pid: %d, name: %s\n", p->pid, p->name);
    }
    panic("kerneltrap");
  }
  
  // give up the CPU if this is a timer interrupt.
  if(scause == CAUSE_INT_TIMER) {
		struct proc *p = myproc();
		if (p != NULL && p->state == RUNNING)
    	yield();
  }
  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// Check if it's an external/software interrupt, 
// and handle it. 
// returns  2 if timer interrupt, 
//          1 if other device, 
//          0 if not recognized. 
int handle_intr(uint64 scause) {
	#ifdef QEMU 
	// handle external interrupt 
	if (scause == CAUSE_INT_EXT) 
	#else 
	// on k210, supervisor software interrupt is used 
	// in alternative to supervisor external interrupt, 
	// which is not available on k210. 
	if (CAUSE_INT_SOFT == scause && 9 == r_stval()) 
	#endif 
	{
		int irq = plic_claim();
		if (UART_IRQ == irq) {
			// keyboard input 
			int c = sbi_console_getchar();
			if (-1 != c) {
				consoleintr(c);
			}
		}
		else if (DISK_IRQ == irq) {
			disk_intr();
		}
		else if (irq) {
			printf("unexpected interrupt irq = %d\n", irq);
		}

		if (irq) { plic_complete(irq);}

		#ifndef QEMU 
		w_sip(r_sip() & ~2);    // clear pending bit
		sbi_set_mie();
		#endif 
	}
	else if (CAUSE_INT_TIMER == scause) {
		timer_tick();
	}
	else if (CAUSE_INT_SOFT == scause) {

	}
	else {
		return -1;
	}
	return 0;
}

int handle_excp(uint64 scause)
{
  // Later implement may handle more cases, such as lazy allocation, mmap
  if (scause == CAUSE_EXC_STRPG) {
    return handle_store_page_fault_cow(r_stval());
  }
	return -1;
}

void trapframedump(struct trapframe *tf)
{
  printf("a0: %p\t", tf->a0);
  printf("a1: %p\t", tf->a1);
  printf("a2: %p\t", tf->a2);
  printf("a3: %p\n", tf->a3);
  printf("a4: %p\t", tf->a4);
  printf("a5: %p\t", tf->a5);
  printf("a6: %p\t", tf->a6);
  printf("a7: %p\n", tf->a7);
  printf("t0: %p\t", tf->t0);
  printf("t1: %p\t", tf->t1);
  printf("t2: %p\t", tf->t2);
  printf("t3: %p\n", tf->t3);
  printf("t4: %p\t", tf->t4);
  printf("t5: %p\t", tf->t5);
  printf("t6: %p\t", tf->t6);
  printf("s0: %p\n", tf->s0);
  printf("s1: %p\t", tf->s1);
  printf("s2: %p\t", tf->s2);
  printf("s3: %p\t", tf->s3);
  printf("s4: %p\n", tf->s4);
  printf("s5: %p\t", tf->s5);
  printf("s6: %p\t", tf->s6);
  printf("s7: %p\t", tf->s7);
  printf("s8: %p\n", tf->s8);
  printf("s9: %p\t", tf->s9);
  printf("s10: %p\t", tf->s10);
  printf("s11: %p\t", tf->s11);
  printf("ra: %p\n", tf->ra);
  printf("sp: %p\t", tf->sp);
  printf("gp: %p\t", tf->gp);
  printf("tp: %p\t", tf->tp);
  printf("epc: %p\n", tf->epc);
}
