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

#ifdef __DEBUG_kmalloc 
	void kmtest(void);
	#define __debug_kmtest() \
		do {kmtest();} while (0) 
#else 
	#define __debug_kmtest() \
		do {} while (0)
#endif 

#endif
