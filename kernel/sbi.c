// SBI Call Implementation
#include "types.h"

#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI 4
#define SBI_REMOTE_FENCE_I 5
#define SBI_REMOTE_SFENCE_VMA 6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN 8

static long sbi_call(unsigned long which, unsigned long arg0, unsigned long arg1, unsigned long arg2) {
    long ret;
    asm volatile (
        "ld x10, %1 \n\t"
        "ld x11, %2 \n\t"
        "ld x12, %3 \n\t"
        "ld x17, %4 \n\t"
        "ecall \n\t"
        "sd x10, %0 \n\t"
        :"=m"(ret)
        :"m"(arg0), "m"(arg1), "m"(arg2), "m"(which)
        :"memory"
    );
    return ret;
}

// put a char in console
void sbi_console_putchar(int ch) {
    sbi_call(SBI_CONSOLE_PUTCHAR, ch, 0, 0);
}

int sbi_console_getchar() {
    return sbi_call(SBI_CONSOLE_GETCHAR, 0, 0, 0);
}

void sbi_set_timer(uint64 stime_value) {
    sbi_call(SBI_SET_TIMER, (unsigned long)stime_value, 0, 0);
}

void sbi_send_ipi(const unsigned long *hart_mask) {
    sbi_call(SBI_SEND_IPI, (unsigned long)hart_mask, 0, 0);
}

void sbi_clear_ipi(void) {
    sbi_call(SBI_CLEAR_IPI, 0, 0, 0);
}

void sbi_remote_fence_i(const unsigned long *hart_mask) {
    sbi_call(SBI_REMOTE_FENCE_I, (unsigned long)hart_mask, 0, 0);
}

void sbi_remote_sfence_vma(const unsigned long *hart_mask,
                           unsigned long start,
                           unsigned long size) {
                               sbi_call(SBI_REMOTE_SFENCE_VMA, (unsigned long)hart_mask, 0, 0);
                           }

void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
                                unsigned long start,
                                unsigned long size,
                                unsigned long asid) {
                                    sbi_call(SBI_REMOTE_SFENCE_VMA_ASID, (unsigned long)hart_mask, 0, 0);
                                }

void sbi_shutdown(void) {
    sbi_call(SBI_SHUTDOWN, 0, 0, 0);
    while (1)
    {
        // unreachable!
    }
}