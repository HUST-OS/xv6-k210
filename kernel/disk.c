#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/defs.h"

void disk_init(void)
{
    #ifdef QEMU
    virtio_disk_init();
    #endif
}

void disk_read(struct buf *b)
{
    #ifdef QEMU
	virtio_disk_rw(b, 0);
    #endif
}

void disk_write(struct buf *b)
{
    #ifdef QEMU
	virtio_disk_rw(b, 1);
    #endif
}

void disk_intr(void)
{
    #ifdef QEMU
    virtio_disk_intr();
    #endif
}