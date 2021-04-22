
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/proc.h"
#include "include/elf.h"
#include "include/fat32.h"
#include "include/kalloc.h"
#include "include/vm.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/syscall.h"

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct dirent *ep, uint offset, uint sz)
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
    if(eread(ep, 0, (uint64)pa, offset+i, n) != n)
      return -1;
  }

  return 0;
}

// All argvs are pointers came from user space, and should be checked by sys_caller
int execve(char *path, char **argv, char **envp)
{
  struct dirent *ep = NULL;
  pagetable_t pagetable = NULL;
  pagetable_t kpagetable = NULL;
  char *buf = NULL;
  struct proc *p = myproc();

  if ((buf = kalloc()) == NULL || copyinstr2(buf, (uint64)path, FAT32_MAX_PATH) < 0
      || (ep = ename(buf)) == NULL) {
    #ifdef DEBUG
    printf("[exec] %s not found\n", path);
    #endif
    goto bad;
  }
  elock(ep);

  // Check ELF header
  struct elfhdr elf;
  if (eread(ep, 0, (uint64) &elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if (elf.magic != ELF_MAGIC)
    goto bad;

  // Make a copy of p->kpt without old user space, 
  // but with the same kstack we are using now, which can't be changed.
  // Load program into memory.
  uint64 sz = 0;
  if ((pagetable = proc_pagetable(p)) == NULL)
    goto bad;
  if ((kpagetable = (pagetable_t)kalloc()) == NULL)
    goto bad;
  memmove(kpagetable, p->kpagetable, PGSIZE);
  for (int i = 0; i < PX(2, MAXUVA); i++) {
    kpagetable[i] = 0;
  }
  struct proghdr ph;
  for (int i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph)) {
    if (eread(ep, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    uint64 sz1;
    if ((sz1 = uvmalloc(pagetable, kpagetable, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    sz = sz1;
    if (ph.vaddr % PGSIZE != 0)
      goto bad;
    if (loadseg(pagetable, ph.vaddr, ep, ph.off, ph.filesz) < 0)
      goto bad;
  }
  char pname[16];
  safestrcpy(pname, ep->filename, sizeof(pname));
  eunlock(ep);
  eput(ep);
  ep = 0;

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
  if (copyout(pagetable, sp, (char *)&ep, sizeof(uint64)) < 0)  // *ep is 0 now, borrow it
    goto bad;

  // Push env strings
  uint64 argc = 0, envc = 0, uargv[MAXARG + 1], uenvp[MAXENV + 1], argp;
  while (envp) {
    if (envc >= MAXENV || fetchaddr((uint64)(envp + envc), &argp) < 0)
      goto bad;
    if (argp == 0)
      break;
    int envlen = fetchstr(argp, buf, PGSIZE);   // '\0' included in PGSIZE, but not in envlen
    if (envlen++ < 0)                                 // including '\0'
      goto bad;
    sp -= envlen;
    sp -= sp % 16;
    if (sp < stackbase)
      goto bad;
    if (copyout(pagetable, sp, buf, envlen) < 0)
      goto bad;
    uenvp[envc++] = sp;
  }
  uenvp[envc] = 0;
  // Push envp[] table
  sp -= (envc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase)
    goto bad;
  if (copyout(pagetable, sp, (char *)uenvp, (envc + 1) * sizeof(uint64)) < 0)
    goto bad;
  p->trapframe->a2 = sp;
  // Push argument strings, prepare rest of stack in ustack.
  while (argv) {
    if (argc >= MAXARG || fetchaddr((uint64)(argv + argc), &argp) < 0)
      goto bad;
    if (argp == 0)
      break;
    int argvlen = fetchstr(argp, buf, PGSIZE);
    if (argvlen++ < 0)
      goto bad;
    sp -= argvlen;
    sp -= sp % 16; // riscv sp must be 16-byte aligned
    if (sp < stackbase)
      goto bad;
    if (copyout(pagetable, sp, buf, argvlen) < 0)
      goto bad;
    uargv[argc++] = sp;
  }
  uargv[argc] = 0;
  // push the array of argv[] pointers.
  sp -= (argc + 1) * sizeof(uint64);
  sp -= sp % 16;
  if (sp < stackbase)
    goto bad;
  if (copyout(pagetable, sp, (char *)uargv, (argc + 1) * sizeof(uint64)) < 0)
    goto bad;

  // arguments to user main(argc, argv)
  // argc is returned via the system call return
  // value, which goes in a0.
  p->trapframe->a1 = sp;

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
  kfree(buf);
  proc_freepagetable(oldpagetable, oldsz);
  w_satp(MAKE_SATP(p->kpagetable));
  sfence_vma();
  kvmfree(oldkpagetable, 0);
  return argc; // this ends up in a0, the first argument to main(argc, argv)

 bad:
  #ifdef DEBUG
  printf("[exec] reach bad\n");
  #endif
  if (buf)
    kfree(buf);
  if (pagetable)
    proc_freepagetable(pagetable, sz);
  if (kpagetable)
    kvmfree(kpagetable, 0);
  if (ep) {
    eunlock(ep);
    eput(ep);
  }
  return -1;
}
