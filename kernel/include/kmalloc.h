#ifndef __KMALLOC_H
#define __KMALLOC_H

#include "types.h"

void            kmallocinit();
void*           kmalloc(uint size);
void            kfree(void *);

void            kmtest();

#endif