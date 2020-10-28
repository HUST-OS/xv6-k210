// Timer Interrupt handler

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"
#include "sbi.h"
#include "memlayout.h"
#include "uarths.h"

static int tick = 0;

void timerinit() {
    // enable supervisor-mode timer interrupts.
    w_sie(r_sie() | SIE_STIE);
    set_next_timeout();
    printf("timerinit\n");
}

void
set_next_timeout() {
    // There is a very strange bug,
    // if comment the `printf` line below
    // the timer will not work.
    printf("");
    sbi_set_timer(r_time() + INTERVAL);
}

void timer_tick() {
    set_next_timeout();
    tick++;
    if((tick % 10) == 0) {
        printf("[Timer]tick: %d\n", tick);
        uint32 c = *(uint32*)(UARTHS + UARTHS_REG_RXFIFO);
        if(c <= 255) {
            printf("[UARTHS]receive: %p, ", c);
            sbi_console_putchar(c);
            printf("\n");
        }
    }
}