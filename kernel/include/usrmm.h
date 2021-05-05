#ifndef __USRMM_H
#define __USRMM_H

#include "riscv.h"
#include "types.h"

enum segtype { CODE, DATA, HEAP, STACK };

struct mem{
  enum segtype type;
  pagetable_t pagetable;
  uint64 sz;
  struct mem *next;
};

/*
 * create a new record, returning -1 at failure
 */
int newrec(struct mem *head, enum segtype type);

/*
 * free the pagetable in the record referenced by p
 */
void freerec(struct mem *p);

/*
 * delete the whole record referenced by pre->next
 */
void delrec(struct mem *pre);
#endif