/* 
 * Physical Page Allocator 
 */

#ifndef __PM_H
#define __PM_H

#include "types.h"

/* init the allocator */
void            kpminit(void);

/* allocate a physical page */
void*           allocpage(void);

/* free an allocated phyiscal page */
void            freepage(void *);

uint64          idlepages(void);

#endif
