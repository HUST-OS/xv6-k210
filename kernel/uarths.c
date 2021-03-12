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

// the transmit output buffer.
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
int uart_tx_w; // write next to uart_tx_buf[uart_tx_w++]
int uart_tx_r; // read next from uart_tx_buf[uar_tx_r++]

struct spinlock uarths_tx_lock;

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
    initlock(&uarths_tx_lock, "uarths");

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

void
uartstart()
{
    while(1){
        if(uart_tx_w == uart_tx_r){
            // transmit buffer is empty.
            return;
        }
        
        if(uarths->txdata.full){
            // the UART transmit holding register is full,
            // so we cannot give it another byte.
            // it will interrupt when it's ready for a new byte.
            return;
        }
        
        int c = uart_tx_buf[uart_tx_r];
        uart_tx_r = (uart_tx_r + 1) % UART_TX_BUF_SIZE;
        
        // maybe uartputc() is waiting for space in the buffer.
        wakeup(&uart_tx_r);
        
        uarths->txdata.data = (uint8)c;
    }
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

  // send buffered characters.
  acquire(&uart_tx_lock);
  uartstart();
  release(&uart_tx_lock);
}
