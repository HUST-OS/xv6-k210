// Timer Interrupt handler


#include "include/types.h"
#include "include/param.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/sbi.h"
#include "include/memlayout.h"
#include "include/uarths.h"

// static int tick = 0;

void timerinit() {
    // enable supervisor-mode timer interrupts.
    w_sie(r_sie() | SIE_STIE);
    set_next_timeout();
    #ifdef DEBUG
    printf("timerinit\n");
    #endif
}

void
set_next_timeout() {
    // There is a very strange bug,
    // if comment the `printf` line below
    // the timer will not work.

    // this bug seems to disappear automatically
    // printf("");
    sbi_set_timer(r_time() + INTERVAL);
}

void timer_tick() {
    set_next_timeout();
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
    #ifndef QEMU
    // warning: not steady!
    uint32 c = *(uint32*)(UARTHS + UARTHS_REG_RXFIFO);
    if (c <= 255) {
        consoleintr(c);
    }
    // if((ticks % 5) == 0) {
        // printf("[Timer]tick: %d from hart %d\n", ticks, r_tp());
        // if(c <= 255) {
        //     printf("[UARTHS]receive: %p, ", c);
        //     sbi_console_putchar(c);
        //     printf("\n");
        // }
    // }
    #endif
}
