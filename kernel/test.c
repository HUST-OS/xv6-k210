// test implemetation

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void test_kalloc() {
    char *mem = kalloc();
    memset(mem, 0, PGSIZE);
    strncpy(mem, "Hello, xv6-k210", 16);
    printf("[test]mem: %s\n", mem);
}