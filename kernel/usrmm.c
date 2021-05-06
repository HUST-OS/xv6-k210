#include "include/usrmm.h"
#include "include/kmalloc.h"
#include "include/proc.h"
#include "include/debug.h"

int
newseg(pagetable_t pagetable, struct seg *head, enum segtype type, uint64 offset, uint64 sz, long flag)
{
  uint64 nstart, nend;
  nstart = type == STACK ? offset - sz : offset;
  nend = type == STACK ? offset : offset + sz;
  while(head)
  {
    uint64 start, end;
    start = head->type == STACK ? head->addr - head->sz : head->addr;
    end = head->type == STACK ? head->addr : head->addr + head->sz;
    if(nstart < start && nend < end)
      break;
    else if(nstart > start && nend > end)
      head = head->next;
    else
    {
      __debug_error("newseg", "segments overlap\n");
	    return -1;
    }
  }

  /* TODO:  there are two ways of accomplish this, one is to alloc here then just 
            add the sz when growing while the other way is to actually alloc each
            time when growing */
  /* TODO:  if the latter chosen, there is no need to distinguish STACK here */
  if((sz = uvmalloc(pagetable, offset, offset + sz, flag)) == 0)
  {
    __debug_error("newseg", "fail to uvmalloc\n");
	  return -1;
  }

  struct seg *p;
  if((p = (struct seg *)kmalloc(sizeof(struct seg))) == NULL)
  {
    __debug_error("newseg", "fail to kmalloc\n");
	  return -1;
  }
  p->next = head->next;
  head->next = p;
  p->addr = offset;
  p->sz = sz;
  p->type = type;
  return 0;
}

enum segtype
typeofseg(struct seg *head, uint64 addr)
{
  enum segtype type = NONE;
  while(head){
    uint64 start, end;
    start = head->type == STACK ? head->addr - head->sz : head->addr;
    end = head->type == STACK ? head->addr : head->addr + head->sz;
    if(addr >= start && addr <= end)
    {
      type = head->type;
      break;
    }
    else if(addr > end)
      head = head->next;
    else{
      __debug_error("typeofseg", "in no segment\n");
	  return -1;
    }
  }

  return type;
}

void 
freeseg(pagetable_t pagetable, struct seg *p)
{
  // TODO:  Whether to distinguish STACK ?
  // TODO:  Not sure about the interface of uvmdealloc.
  uvmdealloc(pagetable, p->addr, p->addr+p->sz);
  p->sz = 0;
}

struct seg*
delseg(pagetable_t pagetable, struct seg *pre)
{
  if(pre->next)
  {
    freeseg(pagetable, pre->next);
    kfree(pre->next);
    return pre;
  }
  else{
    freeseg(pagetable, pre);
    kree(pre);
    return NULL;
  }
}