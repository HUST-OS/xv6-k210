#include "include/usrmm.h"
#include "include/kmalloc.h"
#include "include/proc.h"
#include "include/debug.h"

int
newrec(struct mem *head, enum segtype type)
{
  struct mem *p;
  if((p = (struct mem *)kmalloc(sizeof(struct mem))) == NULL){
    __debug_error("newrec", "fail to kmalloc\n");
	return -1;
  }
  p->next = head->next;
  p->pagetable = NULL;
  p->sz = 0;
  p->type = type;
  return 0;
}

void
freerec(struct mem *p)
{
  if(p->pagetable){
    proc_freepagetable((void*)p->pagetable, p->sz);
    p->pagetable = NULL;
    p->sz = 0;
  }
}

void
delrec(struct mem *pre)
{
  struct mem *tmp = pre->next;
  pre->next = pre->next->next;
  if(tmp->pagetable)
    freerec(tmp);
  kfree(tmp);
}