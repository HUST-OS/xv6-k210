#ifndef __DEBUG_usrmm
#undef  DEBUG
#endif

#include "include/usrmm.h"
#include "include/kmalloc.h"
#include "include/proc.h"
#include "include/vm.h"
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

  struct seg *p;
  if((p = (struct seg *)kmalloc(sizeof(struct seg))) == NULL)
  {
    __debug_error("newseg", "fail to kmalloc\n");
	  return -1;
  }

  /* 
    如果是exec中载入LOAD型的段，则需要完全分配页；
    如果是堆增长，只修改堆对应的seg中的size即可（在sys_sbrk()中调用），页的分配由page fault处理
    如果是堆减小，则需要调用uvmdemalloc（也是sys_sbrk()中调用）
  */
  /*
      现在uvmalloc不需要段类型了，只需要给定权限即可
      注意，ELF中标记的权限和页表项的权限对应的宏应该不一样，需要转换
  */
  if((sz = uvmalloc(pagetable, nstart, nend, flag)) == 0)
  {
    kfree(p);
    __debug_error("newseg", "fail to uvmalloc\n");
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
    if(addr >= start && addr < end)
    {
      type = head->type;
      break;
    }
    else if(addr >= end)
      head = head->next;
    else{
      __debug_error("typeofseg", "in no segment\n");
	  return NONE;
    }
  }

  return type;
}

void 
freeseg(pagetable_t pagetable, struct seg *p)
{
  // 应该不要需要区分栈段，直接把段类型丢进去就行，uvmdemalloc自己判断（暂定）
  uvmdealloc(pagetable, p->addr, p->addr+p->sz, p->type);
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
    kfree(pre);
    return NULL;
  }
}