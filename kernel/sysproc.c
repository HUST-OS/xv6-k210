
#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/timer.h"
#include "include/kalloc.h"
#include "include/string.h"
#include "include/printf.h"

extern int execve(char *path, char **argv, char **envp);

uint64
sys_exec(void)
{
  char path[FAT32_MAX_PATH];
  uint64 argv;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &argv) < 0){
    return -1;
  }

  return execve(path, (char **)argv, 0);
}

uint64
sys_execve(void)
{
  char path[FAT32_MAX_PATH];
  uint64 argv, envp;

  if(argstr(0, path, FAT32_MAX_PATH) < 0 || argaddr(1, &argv) < 0 || argaddr(2, &envp)){
    return -1;
  }

  return execve(path, (char **)argv, (char **)envp);
}

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  // since exit never return, we print the trace-info here
  if (myproc()->tmask & (1 << (SYS_exit - 1))) {
    printf(")\n");
  }
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  // since wait suspends the proc, we print the left trace-info here
  // and when coming back, we re-print the leading trace-info for a clear view
  struct proc *pr = myproc();
  int mask = pr->tmask & (1 << (SYS_wait - 1));
  if (mask) {
    printf(") ...\n");
  }
  int ret = wait(p);
  if (mask) {
    printf("pid %d: return from wait(0x%x", pr->pid, p);
  }
  return ret;
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  struct proc *p = myproc();
  // acquire(&tickslock);
  acquire(&p->lock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(p->killed){
      // release(&tickslock);
      release(&p->lock);
      return -1;
    }
    // sleep(&ticks, &tickslock);
    sleep(&ticks, &p->lock);
  }
  // release(&tickslock);
  release(&p->lock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0) {
    return -1;
  }
  myproc()->tmask = mask;
  return 0;
}