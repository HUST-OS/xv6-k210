
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/sbi.h"
#include "include/plic.h"

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void
plicinit(void)
{
  // set desired IRQ priorities non-zero (otherwise disabled).
  #ifndef QEMU
  writel(1, PLIC + IRQN_UARTHS_INTERRUPT * sizeof(uint32));
  writel(1, PLIC + IRQN_DMA0_INTERRUPT * sizeof(uint32));
  #else
  *(uint32*)(PLIC + UART0_IRQ*4) = 1;
  *(uint32*)(PLIC + VIRTIO0_IRQ*4) = 1;
  #endif
  #ifdef DEBUG
  printf("plicinit\n");
  #endif
}

void
plicinithart(void)
{
  int hart = cpuid();
  #ifdef QEMU
  // set uart's enable bit for this hart's S-mode. 
  *(uint32*)PLIC_SENABLE(hart)= (1 << UART0_IRQ) | (1 << VIRTIO0_IRQ);
  // set this hart's S-mode priority threshold to 0.
  *(uint32*)PLIC_SPRIORITY(hart) = 0;
  #else
  uint32 *hart_m_enable = (uint32*)PLIC_MENABLE(hart);
  *(hart_m_enable) = readl(hart_m_enable) | (1 << IRQN_DMA0_INTERRUPT);
  uint32 *hart0_m_int_enable_hi = hart_m_enable + 1;
  *(hart0_m_int_enable_hi) = readl(hart0_m_int_enable_hi) | (1 << (IRQN_UARTHS_INTERRUPT % 32));
  extern void supervisor_external_handler();
  sbi_set_extern_interrupt((uint64)supervisor_external_handler);
  #endif
  #ifdef DEBUG
  printf("plicinithart\n");
  #endif
}

// ask the PLIC what interrupt we should serve.
int
plic_claim(void)
{
  int hart = cpuid();
  #ifndef QEMU
  int irq = *(uint32*)PLIC_MCLAIM(hart);
  #else
  int irq = *(uint32*)PLIC_SCLAIM(hart);
  #endif
  return irq;
}

// tell the PLIC we've served this IRQ.
void
plic_complete(int irq)
{
  int hart = cpuid();
  #ifndef QEMU
  *(uint32*)PLIC_MCLAIM(hart) = irq;
  #else
  *(uint32*)PLIC_SCLAIM(hart) = irq;
  #endif
}

