#ifndef __KMALLOC_H
#define __KMALLOC_H

#include "types.h"

/* 
 * init allocator 
 */
void 			kmallocinit(void);

/* 
 * allocate a range of mem of wanted size 
 */
void*           kmalloc(uint size);

/* 
 * free memory starts with `addr`
 */
void            kfree(void *addr);

#ifdef DEBUG 
void            kmtest();
#endif 

#endif
