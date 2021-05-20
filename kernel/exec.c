
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/proc.h"
#include "include/elf.h"
#include "include/fs.h"
#include "include/pm.h"
#include "include/vm.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/syscall.h"

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
    if (arglen++ < 0)                                 // including '\0'
      goto bad;
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
  pagetable_t kpagetable = NULL;
  struct proc *p = myproc();

  if ((ip = namei(path)) == NULL)
    goto bad;
  ilock(ip);

  // Check ELF header
  struct elfhdr elf;
  if (ip->fop->read(ip, 0, (uint64) &elf, 0, sizeof(elf)) != sizeof(elf) || elf.magic != ELF_MAGIC)
    goto bad;

  // Make a copy of p->kpt without old user space, 
  // but with the same kstack we are using now, which can't be changed.
  // Load program into memory.
  uint64 sz = 0;
  if ((pagetable = proc_pagetable(p)) == NULL ||
      (kpagetable = (pagetable_t)allocpage()) == NULL)
    goto bad;
  memmove(kpagetable, p->kpagetable, PGSIZE);
  for (int i = 0; i < PX(2, MAXUVA); i++) {
    kpagetable[i] = 0;
  }
  struct proghdr ph;
  for (int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (ip->fop->read(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz || ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if ((sz1 = uvmalloc(pagetable, kpagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    sz = sz1;
    if (ph.vaddr % PGSIZE != 0)
      goto bad;
    if (loadseg(pagetable, ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }

  struct stat st;
  ip->op->getattr(ip, &st);
  char pname[16];
  safestrcpy(pname, st.name, sizeof(pname));
  iunlock(ip);
  iput(ip);
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Use the second as the user stack.
  sz = PGROUNDUP(sz);
  uint64 sz1;
  if ((sz1 = uvmalloc(pagetable, kpagetable, sz, sz + 2 * PGSIZE)) == 0)
    goto bad;
  sz = sz1;
  uvmclear(pagetable, sz - 2 * PGSIZE);
  uint64 sp = sz;
  uint64 stackbase = sp - PGSIZE;
  sp -= sizeof(uint64);
  if (copyout(pagetable, sp, (char *)&ip, sizeof(uint64)) < 0)  // *ep is 0 now, borrow it
    goto bad;

  // arguments to user main(argc, argv, envp)
  // argc is returned via the system call return
  // value, which goes in a0.
  int argc, envc;
  uint64 uargv[MAXARG + 1], uenvp[MAXENV + 1];
  if ((envc = pushstack(pagetable, uenvp, envp, MAXENV, &sp)) < 0 ||
      (argc = pushstack(pagetable, uargv, argv, MAXARG, &sp)) < 0)
    goto bad;
  sp -= (envc + 1) * sizeof(uint64);
  uint64 a2 = sp;
  sp -= (argc + 1) * sizeof(uint64);
  if (sp < stackbase || 
      copyout(pagetable, a2, (char *)uenvp, (envc + 1) * sizeof(uint64)) < 0 ||
      copyout(pagetable, sp, (char *)uargv, (argc + 1) * sizeof(uint64)) < 0)
    goto bad;
  p->trapframe->a1 = sp;
  p->trapframe->a2 = a2;

  // Save program name for debugging.
  memmove(p->name, pname, sizeof(p->name));

  // Commit to the user image.
  pagetable_t oldpagetable = p->pagetable;
  pagetable_t oldkpagetable = p->kpagetable;
  uint64 oldsz = p->sz;
  p->pagetable = pagetable;
  p->kpagetable = kpagetable;
  p->sz = sz;
  p->trapframe->epc = elf.entry;  // initial program counter = main
  p->trapframe->sp = sp; // initial stack pointer
  proc_freepagetable(oldpagetable, oldsz);
  w_satp(MAKE_SATP(p->kpagetable));
  sfence_vma();
  kvmfree(oldkpagetable, 0);

  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  if (pagetable)
    proc_freepagetable(pagetable, sz);
  if (kpagetable)
    kvmfree(kpagetable, 0);
  if (ip) {
    iunlock(ip);
    iput(ip);
  }
  return -1;
}
