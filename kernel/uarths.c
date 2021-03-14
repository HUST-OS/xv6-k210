// uarths for k210
// for now the solution of handling ext-irq approach is to
// run S-mode code in M-mode directly (which is bad, but
// nothing else compared at present).
// so we can not use sbi putchar when handling consoleintr

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/defs.h"
#include "include/intr.h"
#include "include/uarths.h"
#include "include/sysctl.h"

extern volatile int panicked; // from printf.c

volatile uarths_t *const uarths = (volatile uarths_t *)UARTHS;

void
uartinit(void)
{
    uint32 freq = sysctl_clock_get_freq(SYSCTL_CLOCK_CPU);
    uint16 div = freq / 115200 - 1;

    /* Set UART registers */
    uarths->div.div = div;
    uarths->txctrl.txen = 1;
    uarths->rxctrl.rxen = 1;
    uarths->txctrl.txcnt = 0;
    uarths->rxctrl.rxcnt = 0;
    uarths->ip.txwm = 1;
    uarths->ip.rxwm = 1;
    uarths->ie.txwm = 0;
    uarths->ie.rxwm = 1;
}

void
uartputc_sync(int c)
{
    push_off();

    if(panicked){
        for(;;)
        ;
    }

    // wait for Transmit Holding Empty to be set in LSR.
    while(uarths->txdata.full)
        continue;
    uarths->txdata.data = (uint8)c;

    pop_off();
}

// read one input character from the UART.
// return -1 if none is waiting.
int
uartgetc(void)
{
  uarths_rxdata_t recv = uarths->rxdata;

  if(recv.empty)
      return -1;
  else
      return (recv.data & 0xff);
}

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from trap.c.
void
uartintr(void)
{
  // read and process incoming characters.
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }
}
