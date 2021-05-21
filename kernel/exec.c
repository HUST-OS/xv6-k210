#ifndef __DEBUG_exec
#undef  DEBUG
#endif

#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/proc.h"
#include "include/elf.h"
#include "include/fs.h"
#include "include/pm.h"
#include "include/vm.h"
#include "include/usrmm.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/syscall.h"
#include "include/debug.h"

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint offset, uint sz)
{
  uint i, n;
  uint64 pa;
  if((va % PGSIZE) != 0)
    panic("loadseg: va must be page aligned");

  for(i = 0; i < sz; i += PGSIZE){
    pa = walkaddr(pagetable, va + i);
    if(pa == NULL)
      panic("loadseg: address should exist");
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(ip->fop->read(ip, 0, (uint64)pa, offset + i, n) != n)
      return -1;
  }

  return 0;
}

// push arg or env strings into user stack, return strings count
static int pushstack(pagetable_t pt, uint64 table[], char **utable, int maxc, uint64 *spp)
{
  uint64 sp = *spp;
  uint64 stackbase = PGROUNDDOWN(sp);
  uint64 argc, argp;
  char *buf;

  if ((buf = allocpage()) == NULL) {
    return -1;
  }
  for (argc = 0; utable; argc++) {
    if (argc >= maxc || fetchaddr((uint64)(utable + argc), &argp) < 0)
      goto bad;
    if (argp == 0)
      break;
    int arglen = fetchstr(argp, buf, PGSIZE);   // '\0' included in PGSIZE, but not in envlen
    if (arglen++ < 0) {                               // including '\0'
      // __debug_warn("pushstack", "didn't get null\n");
      goto bad;
    }
    sp -= arglen;
    sp -= sp % 16;
    if (sp < stackbase || copyout(pt, sp, buf, arglen) < 0)
      goto bad;
    table[argc] = sp;
  }
  table[argc] = 0;
  *spp = sp;
  freepage(buf);
  return argc;

bad:
  if (buf)
    freepage(buf);
  return -1;
}

// All argvs are pointers came from user space, and should be checked by sys_caller
int execve(char *path, char **argv, char **envp)
{
  struct inode *ip = NULL;
  pagetable_t pagetable = NULL;
  struct seg *seghead = NULL, *seg;
  struct proc *p = myproc();

  if ((ip = namei(path)) == NULL) {
    __debug_warn("execve", "can't open %s\n", path);
    goto bad;
  }
  ilock(ip);

  // Check ELF header
  struct elfhdr elf;
  if (ip->fop->read(ip, 0, (uint64) &elf, 0, sizeof(elf)) != sizeof(elf) || elf.magic != ELF_MAGIC)
    goto bad;

  // Make a copy of p->pagetable without old user space, 
  // but with the same kstack we are using now, which can't be changed.
  if ((pagetable = (pagetable_t)allocpage()) == NULL)
    goto bad;
  memmove(pagetable, p->pagetable, PGSIZE);
  // Remove old u-map from the new pt, but don't free since later op might fail.
  for (int i = 0; i < PX(2, MAXUVA); i++) {
    pagetable[i] = 0;
  }

  // Load program into memory.
  struct proghdr ph;
  int flags;
  for (int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (ip->fop->read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph)) {
      __debug_warn("execve", "fail to read ELF file\n");
      goto bad;
    }
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.vaddr % PGSIZE != 0 || ph.memsz < ph.filesz || ph.vaddr + ph.memsz < ph.vaddr) {
      __debug_warn("execve", "funny ELF file: va=%p msz=0x%x fsz=0x%x\n", ph.vaddr, ph.memsz, ph.filesz);
      goto bad;
    }
    
    // turn ELF flags to PTE flags
    flags = 0;
    flags |= (ph.flags & ELF_PROG_FLAG_EXEC) ? PTE_X : 0;
    flags |= (ph.flags & ELF_PROG_FLAG_WRITE) ? PTE_W : 0;
    flags |= (ph.flags & ELF_PROG_FLAG_READ) ? PTE_R : 0;

    __debug_info("execve", "newseg in\n");
    seg = newseg(pagetable, seghead, LOAD, ph.vaddr, ph.memsz, flags);
    __debug_info("execve", "newseg out\n");
    if(seg == NULL) {
      __debug_warn("execve", "newseg fail: vaddr=%p, memsz=%d\n", ph.vaddr, ph.memsz);
      goto bad;
    }

    seghead = seg;

    if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0) {
      __debug_warn("execve", "load segment\n");
      goto bad;
    }
  }

  struct stat st;
  ip->op->getattr(ip, &st);
  char pname[16];
  safestrcpy(pname, st.name, sizeof(pname));
  iunlock(ip);
  iput(ip);
  ip = 0;

  /*---------------------------*/
  /* TODO:
  Call to usrmm to get a STACK segment struct and allocate pages
  The problem is how to locate the stack, may we can place it near MAXUVA
  */
  // Original Code:
  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  // Clear the PTE_U mark of the first page under the stack as a protection.
  // if ((sz1 = uvmalloc(pagetable, sz, sz + 2 * PGSIZE, PTE_W|PTE_R|PTE_X)) == 0)
  // Heap
  flags = PTE_R | PTE_W;
  for (seg = seghead; seg->next != NULL; seg = seg->next);
    __debug_info("execve", "making heap, start=%p\n", PGROUNDUP(seg->addr + seg->sz));
  seg = newseg(pagetable, seghead, HEAP, PGROUNDUP(seg->addr + seg->sz), 0, flags);
  if(seg == NULL) {
    __debug_warn("execve", "new heap fail\n");
    goto bad;
  }
  seghead = seg;

    __debug_info("execve", "making stack\n");
  // Stack
  seg = newseg(pagetable, seghead, STACK, VUSTACK - PGSIZE, PGSIZE, flags);
  if(seg == NULL) {
    __debug_warn("execve", "new stack fail\n");
    goto bad;
  }
  seghead = seg;
  // uvmclear(pagetable, sz - 2 * PGSIZE);

  __debug_info("execve", "pushing stack\n");
  /* If the stack is located, we can assign sp */
  uint64 sp = VUSTACK;
  uint64 stackbase = VUSTACK - PGSIZE;

  sp -= sizeof(uint64);
  sp -= sp % 16;        // on risc-v, sp should be 16-byte aligned

  if (copyout(pagetable, sp, (char *)&ip, sizeof(uint64)) < 0) {  // *ep is 0 now, borrow it
    __debug_warn("execve", "fail to push bottom NULL into user stack\n");
    goto bad;
  }

  __debug_info("execve", "pushing argv/envp\n");
  // arguments to user main(argc, argv, envp)
  // argc is returned via the system call return
  // value, which goes in a0.
  int argc, envc;
  uint64 uargv[MAXARG + 1], uenvp[MAXENV + 1];
  if ((envc = pushstack(pagetable, uenvp, envp, MAXENV, &sp)) < 0 ||
      (argc = pushstack(pagetable, uargv, argv, MAXARG, &sp)) < 0) {
    __debug_warn("execve", "fail to push argv or envp into user stack\n");
    goto bad;
  }
  sp -= (envc + 1) * sizeof(uint64);
  sp -= sp % 16;
  uint64 a2 = sp;
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  __debug_info("execve", "pushing argv/envp table\n");
  if (sp < stackbase || 
      copyout(pagetable, a2, (char *)uenvp, (envc + 1) * sizeof(uint64)) < 0 ||
      copyout(pagetable, sp, (char *)uargv, (argc + 1) * sizeof(uint64)) < 0)
  {
    __debug_warn("execve", "fail to copy argv/envp table into user stack\n");
    goto bad;
  }
  p->trapframe->a1 = sp;
  p->trapframe->a2 = a2;

  // Save program name for debugging.
  memmove(p->name, pname, sizeof(p->name));

  // Commit to the user image.
  pagetable_t oldpagetable = p->pagetable;
  seg = p->segment;
  p->pagetable = pagetable;
  p->segment = seghead;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer

  __debug_info("execve", "sp=%p, stackbase=%p\n", sp, stackbase);
  w_satp(MAKE_SATP(p->pagetable));
  sfence_vma();

  delsegs(oldpagetable, seg);
  uvmfree(oldpagetable);
  freepage(oldpagetable);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  __debug_warn("execve", "reach bad: seg=%p, pt=%p, ep=%p\n", seghead, pagetable, ep);
  __debug_warn("execve", "reach bad: seg=%p, pt=%p, ep=%p\n", seghead, pagetable, ep);
  if (seghead) {
    delsegs(pagetable, seghead);
  }
  if (pagetable) {
    uvmfree(pagetable);
    freepage(pagetable);
  }
  if (ip) {
    iunlock(ip);
    iput(ip);
  }
  return -1;
}
