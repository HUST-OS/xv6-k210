#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "sbi.h"

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void
plicinit(void)
{
  // set desired IRQ priorities non-zero (otherwise disabled).
  *(uint32*)(PLIC + UART0_IRQ*4) = 1;
  *(uint32*)(PLIC + VIRTIO0_IRQ*4) = 1;
  printf("plicinit\n");
}

void
plicinithart(void)
{
  int hart = cpuid();
  
  // set uart's enable bit for this hart's S-mode. 
  *(uint32*)PLIC_SENABLE(hart)= (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);

  // set this hart's S-mode priority threshold to 0.
  *(uint32*)PLIC_SPRIORITY(hart) = 0;
  printf("plicinithart\n");
}

// ask the PLIC what interrupt we should serve.
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(uint32*)PLIC_SCLAIM(hart);
  return irq;
}

// tell the PLIC we've served this IRQ.
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(uint32*)PLIC_SCLAIM(hart) = irq;
}

void device_init(unsigned long pa, uint64 hartid) {
  // after RustSBI, txen = rxen = 1, rxie = 1, rxcnt = 0
  // start UART interrupt configuration
  // disable external interrupt on hart1 by setting threshold
  uint32 *hart0_m_threshold = (uint32*)PLIC;
  uint32 *hart1_m_threshold = (uint32*)PLIC_MENABLE(hartid);
  *(hart0_m_threshold) = 0;
  *(hart1_m_threshold) = 1;
  // *(uint32*)0x0c200000 = 0;
  // *(uint32*)0x0c202000 = 1;

  // now using UARTHS whose IRQID = 33
  // assure that its priority equals 1
  // if(*(uint32*)(0x0c000000 + 33 * 4) != 1) panic("uarhs's priority is not 1\n");
  // printf("uart priority: %p\n", *(uint32*)(0x0c000000 + 33 * 4));
  // *(uint32*)(0x0c000000 + 33 * 4) = 0x1;
  uint32 *hart0_m_int_enable_hi = (uint32*)(PLIC_MENABLE(hartid) + 0x04);
  *(hart0_m_int_enable_hi) = (1 << 0x1);
  // *(uint32*)0x0c002004 = (1 << 0x1);
  sbi_set_extern_interrupt((uint64)supervisor_external_handler - 0xffffffff00000000);
  printf("device init\n");
}