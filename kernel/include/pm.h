#ifndef __PM_H
#define __PM_H

#include "types.h"

void            kpminit(void);
void*           allocpage(void);
void            freepage(void *);
uint64          idlepages(void);

#endif