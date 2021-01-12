// 
// driver for qemu's virtio disk device 
// uses qemu's mmio interface to virtio. 
// qemu presents a "legacy" virtio interface. 
//
// qemu ... -drive file=fs.img,if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

#include "include/types.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/sleeplock.h"
#include "include/fs.h"
#include "include/buf.h"
#include "include/virtio.h"

#define R(r) \
	((volatile uint32 *)(VIRTIO0 + (r)))

static struct {
	// three queues for descriptors 
	char pages[2 * PGSIZE];
	struct VRingDesc *desc;	// the desc queue 
	uint16 *avail; 			// the avail queue 
	struct UsedArea *used;	// the used queue 

	uint8 free[NUM];		// mark which descriptor is free 
	struct {
		struct buf *b;
		char status;
	} info[NUM];

	uint16 used_idx;

	struct spinlock vdisk_lock;
} __attribute__ ((aligned(PGSIZE))) disk;

/* 
 * Init the virtio disk 
 */
void disk_init(void) {
	uint32 status = 0;

	initlock(&disk.vdisk_lock, "virtio_disk");

	// check virtio_disk 
	if (0x74726976 != *R(VIRTIO_MMIO_MAGIC_VALUE) ||
			1 != *R(VIRTIO_MMIO_VERSION) ||
			2 != *R(VIRTIO_MMIO_DEVICE_ID) ||
			0x554d4551 != *R(VIRTIO_MMIO_VENDOR_ID)) {
		panic("could not find virtio_disk");
	}

	// setup virtio disk 
	
	status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
	*R(VIRTIO_MMIO_STATUS) = status;

	status |= VIRTIO_CONFIG_S_DRIVER;
	*R(VIRTIO_MMIO_STATUS) = status;

	// negotiate features 
	uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
	features &= ~(1 << VIRTIO_BLK_F_RO);
	features &= ~(1 << VIRTIO_BLK_F_SCSI);
	features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
	features &= ~(1 << VIRTIO_BLK_F_MQ);
	features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
	features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
	features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
	*R(VIRTIO_MMIO_DRIVER_FEATURES) = features;
	// negotiate finish 
	status |= VIRTIO_CONFIG_S_FEATURES_OK;
	do {
		*R(VIRTIO_MMIO_STATUS) = status;
	} while (!(*R(VIRTIO_MMIO_STATUS) & VIRTIO_CONFIG_S_FEATURES_OK));

	// tell device we are completely ready 
	status |= VIRTIO_CONFIG_S_DRIVER_OK;
	*R(VIRTIO_MMIO_STATUS) = status;

	*R(VIRTIO_MMIO_GUEST_PAGE_SIZE) = PGSIZE;

	// initialize virtreq queue 
	*R(VIRTIO_MMIO_QUEUE_SEL) = 0;
	uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (0 == max) 
		panic("virtio disk has no queue 0");
	else if (NUM > max) 
		panic("virtio disk max queue too short");
	else {
		*R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
		*R(VIRTIO_MMIO_QUEUE_PFN) = ((uint64)disk.pages) >> PGSHIFT;
	}

	memset(disk.pages, 0, sizeof(disk.pages));
	disk.desc = (struct VRingDesc*)disk.pages;
	disk.avail = (uint16*)((char*)disk.desc + NUM * sizeof(struct VRingDesc));
	disk.used = (struct UsedArea*)(disk.pages + PGSIZE);

	for (int i = 0; i < NUM; i ++) 
		disk.free[i] = 1;

	// plic.c and trap.c arrange for interrupts from VIRTIO0_IRQ 
	printf("virtio_disk_init\n");
}

// alloc a free descriptor 
static int 
alloc_desc(void) {
	for (int i = 0; i < NUM; i ++) {
		if (disk.free[i]) {
			disk.free[i] = 0;
			return i;
		}
	}

	return -1;
}

// free a used descriptor 
static void 
free_desc(int desc) {
	if (NUM <= desc || 0 > desc) 
		panic("invalid desc");
	else if (disk.free[desc]) 
		panic("desc not used");
	else {
		disk.desc[desc].addr = 0;
		disk.free[desc] = 1;

		// sleeplock not added yet 
	}
}

// allocate 3 descriptors 
static int 
alloc3_desc(int *arr) {
	for (int i = 0; i < 3; i ++) {
		arr[i] = alloc_desc();
		if (-1 == arr[i]) {
			for (int j = 0; j < i; j ++) 
				free_desc(arr[j]);
			return -1;
		}
	}
	return 0;
}

// free a chain of descriptors.
static void
free_chain(int i)
{
  while(1){
    free_desc(i);
    if(disk.desc[i].flags & VRING_DESC_F_NEXT)
      i = disk.desc[i].next;
    else
      break;
  }
}

struct {
	uint32 type;
	uint32 reserved;
	uint64 sector;
} tmp;

// the interrupt handler 
static void
disk_intr(void) {
  //acquire(&disk.vdisk_lock);

  while((disk.used_idx % NUM) != (disk.used->id % NUM)){
    int id = disk.used->elems[disk.used_idx].id;

    if(disk.info[id].status != 0)
      panic("virtio_disk_intr status");
    
    disk.info[id].b->disk = 0;   // disk is done with buf
    //wakeup(disk.info[id].b);

    disk.used_idx = (disk.used_idx + 1) % NUM;
  }
  *R(VIRTIO_MMIO_INTERRUPT_ACK) = *R(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3;

  //release(&disk.vdisk_lock);
}

static void 
virtio_disk_rw(struct buf *b, int write) {
	int arr[3];

	acquire(&disk.vdisk_lock);

	if (write) 
		tmp.type = VIRTIO_BLK_T_OUT;	// write the disk 
	else 
		tmp.type = VIRTIO_BLK_T_IN;	// read from disk 
	tmp.reserved = 0;
	tmp.sector = b->sectorno;

	// setup virtreq queue descriptors 
	if (alloc3_desc(arr) == -1) 
		panic("fail to alloc descriptors");

	// make descriptors a list 
	disk.desc[0].next = arr[1];
	disk.desc[1].next = arr[2];
	disk.desc[2].next = 0;

	disk.desc[arr[0]].addr = (uint64)&tmp;
	disk.desc[arr[0]].len = sizeof(tmp);
	disk.desc[arr[0]].flags = VRING_DESC_F_NEXT;

	disk.desc[arr[1]].addr = (uint64)b->data;
	disk.desc[arr[1]].len = BSIZE;
	disk.desc[arr[1]].flags = write ? 0 : 
			VRING_DESC_F_WRITE;
	disk.desc[arr[1]].flags |= VRING_DESC_F_NEXT;

	disk.desc[arr[2]].addr = (uint64)&disk.info[arr[0]].status;
	disk.desc[arr[2]].len = 1;
	disk.desc[arr[2]].flags = VRING_DESC_F_WRITE;

	// record struct buf for interrupt 
	disk.info[arr[0]].status = 0;
	disk.info[arr[0]].b = b;
	b->disk = 1;				// the buffer is occupied by disk 

	// add descriptors into avail queue 
	disk.avail[2 + disk.avail[1]] = arr[0];
	__sync_synchronize();
	disk.avail[1] = (disk.avail[1] + 1) % NUM;

	// inform device that avail queue is updated 
	*R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;

	for (int i = 0; i < 0x7fff; i ++) 
		;
	disk_intr();
	// waiting for interrupt 
	//while (1 == b->disk) 
	//	;		// as for user process, a sleeplock is needed 

	disk.info[arr[0]].b = 0;
	free_chain(arr[0]);

	release(&disk.vdisk_lock);
}

/* 
 * Read a sector from disk 
 */
void 
disk_read(struct buf *b) {
	virtio_disk_rw(b, 0);
}

/* 
 * Write content into disk 
 */
void disk_write(struct buf *b) {
	virtio_disk_rw(b, 1);
}
