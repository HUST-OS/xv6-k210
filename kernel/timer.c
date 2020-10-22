// Timer Interrupt handler

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"

static int tick = 0;
void timerinit() {
    // enable supervisor-mode timer interrupts.
    w_sie(r_sie() | SIE_STIE);
    set_next_timeout();
    printf("timerinit\n");
}

void supervisor_timer() {
    timer_tick();
}

void set_next_timeout() {
    printf("[Timer]read_time: %d\n", r_time());
    sbi_set_timer(r_time() + INTERVAL);
}

void timer_tick() {
    printf("[Timer]tick\n");
    set_next_timeout();
    tick++;
    if(tick % 100 == 0) printf("[Timer]tick\n");
}