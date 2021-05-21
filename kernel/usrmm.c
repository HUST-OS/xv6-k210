#ifndef __DEBUG_usrmm
#undef  DEBUG
#endif

#include "include/usrmm.h"
#include "include/kmalloc.h"
#include "include/proc.h"
#include "include/vm.h"
#include "include/debug.h"

struct seg*
newseg(pagetable_t pagetable, struct seg *head, enum segtype type, uint64 offset, uint64 sz, long flag)
{
  uint64 nstart, nend;
  nstart = offset;
  nend = offset + sz;

  __debug_info("newseg", "type=%d nstart=%p nend=%p\n", type, nstart, nend);
  // __debug_info("newseg", "sizeof(struct seg): %d\n", sizeof(struct seg));
  struct seg *seg = NULL;
  for(struct seg *s = head; s != NULL; s = s->next)
  {
    uint64 start, end;
    start = s->addr;
    end = s->addr + s->sz;
    // __debug_info("newseg", "type=%d start=%p end=%p\n", s->type, start, end);
    if(nend <= start)
    {
      break;
    }
    else if(nstart >= end){
      seg = s;
    }
    else
    {
      __debug_error("newseg", "segments overlap\n");
	    return NULL;
    }
  }

  // __debug_info("newseg", "calling to kmalloc\n");
  struct seg *p;
  if((p = (struct seg *)kmalloc(sizeof(struct seg))) == NULL)
  {
    __debug_error("newseg", "fail to kmalloc\n");
	  return NULL;
  }

  // __debug_info("newseg", "calling to uvmalloc\n");
  if(uvmalloc(pagetable, nstart, nend, flag) == 0)
  {
    kfree(p);
    __debug_error("newseg", "fail to uvmalloc\n");
	  return NULL;
  }

  if(seg == NULL)
  {
    p->next = head;
    head = p;
  }
  else
  {
    p->next = seg->next;
    seg->next = p;
  }
  p->addr = offset;
  p->sz = sz;
  p->type = type;
  p->flag = flag;

  // __debug_info("newseg", "done\n");
  // return p;
  return head;
}

enum segtype
typeofseg(struct seg *head, uint64 addr)
{
  // enum segtype type = NONE;
  // while(head){
  //   uint64 start, end;
  //   start = head->type == STACK ? head->addr - head->sz : head->addr;
  //   end = head->type == STACK ? head->addr : head->addr + head->sz;
  //   if(addr >= start && addr < end)
  //   {
  //     type = head->type;
  //     break;
  //   }
  //   else if(addr >= end)
  //     head = head->next;
  //   else{
  //     __debug_error("typeofseg", "in no segment\n");
	//   return NONE;
  //   }
  // }

  // __debug_info("typeofseg", "addr=%p\n", addr);
  head = locateseg(head, addr);

  return head == NULL ? NONE : head->type;
}

struct seg*
locateseg(struct seg *head, uint64 addr)
{
    // __debug_info("locateseg", "addr=%p\n", addr);
  while(head){
    uint64 start, end;
    start = head->addr;
    end = head->addr + head->sz;
    // __debug_info("locateseg", "type=%d start=%p end=%p\n", head->type, start, end);
    if(addr >= start && addr < end)
    {
      return head;
    }
    else if(addr >= end)
      head = head->next;
    else{
      __debug_error("locateseg", "va=%p in no segment\n", addr);
	  return NULL;
    }
  }

  return NULL;
}

struct seg*
getseg(struct seg *head, enum segtype type)
{
  for (struct seg *seg = head; seg != NULL; seg = seg->next) {
    if (seg->type == type)
      return seg;
  }
  return NULL;
}

// end is not included
enum segtype
partofseg(struct seg *head, uint64 start, uint64 end)
{
  __debug_info("partofseg", "start=%p end=%p\n", start, end);
  enum segtype st = typeofseg(head, start);
  if (st == NONE)
    return NONE;

  return st == typeofseg(head, end - 1) ? st : NONE;
}

void 
freeseg(pagetable_t pagetable, struct seg *p)
{
  uvmdealloc(pagetable, p->addr, p->addr+p->sz, p->type);
  p->sz = 0;
}

struct seg*
delseg(pagetable_t pagetable, struct seg *s)
{
  struct seg *next = s->next;
  uvmdealloc(pagetable, s->addr, s->addr + s->sz, s->type);
  kfree(s);
  return next;
}

void
delsegs(pagetable_t pagetable, struct seg *head){
  while(head){
    head = delseg(pagetable, head);
  }
}