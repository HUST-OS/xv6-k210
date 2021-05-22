
#include "include/types.h"
#include "include/param.h"
#include "include/memlayout.h"
#include "include/riscv.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/syscall.h"
#include "include/sysinfo.h"
#include "include/pm.h"
#include "include/vm.h"
#include "include/usrmm.h"
#include "include/string.h"
#include "include/printf.h"

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();

  if (partofseg(p->segment, addr, addr + sizeof(uint64)) == NONE)
    return -1;
  // if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
  if(copyin2((char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// Returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  // struct proc *p = myproc();
  // int err = copyinstr(p->pagetable, buf, addr, max);
  int ret = copyinstr2(buf, addr, max);
  return ret;
  // if(err < 0)
  //   return err;
  // return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  struct proc *p = myproc();
  if (p->tmask & (1 << (p->trapframe->a7 - 1))) {
    if (n != 0) {
      printf(", ");
    }
    printf("%d", *ip);
  }
  return 0;
}

// Retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  struct proc *p = myproc();
  if (p->tmask & (1 << (p->trapframe->a7 - 1))) {
      if (n != 0) {
      printf(", ");
    }
    printf("0x%x", *ip);
  }
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// Returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  int ret = fetchstr(addr, buf, max);
  struct proc *p = myproc();
  if (ret >= 0 && (p->tmask & (1 << (p->trapframe->a7 - 1)))) {
    printf("=\"%s\"", buf);
  }
  return ret;
}

extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_test_proc(void);
extern uint64 sys_dev(void);
extern uint64 sys_readdir(void);
extern uint64 sys_getcwd(void);
extern uint64 sys_unlink(void);
extern uint64 sys_trace(void);
extern uint64 sys_sysinfo(void);
extern uint64 sys_rename(void);
extern uint64 sys_execve(void);
extern uint64 sys_mount(void);
extern uint64 sys_umount(void);

static uint64 (*syscalls[])(void) = {
  [SYS_fork]        sys_fork,
  [SYS_exit]        sys_exit,
  [SYS_wait]        sys_wait,
  [SYS_pipe]        sys_pipe,
  [SYS_read]        sys_read,
  [SYS_kill]        sys_kill,
  [SYS_exec]        sys_exec,
  [SYS_fstat]       sys_fstat,
  [SYS_chdir]       sys_chdir,
  [SYS_dup]         sys_dup,
  [SYS_getpid]      sys_getpid,
  [SYS_sbrk]        sys_sbrk,
  [SYS_sleep]       sys_sleep,
  [SYS_uptime]      sys_uptime,
  [SYS_open]        sys_open,
  [SYS_write]       sys_write,
  [SYS_mkdir]       sys_mkdir,
  [SYS_close]       sys_close,
  [SYS_test_proc]   sys_test_proc,
  [SYS_dev]         sys_dev,
  [SYS_readdir]     sys_readdir,
  [SYS_getcwd]      sys_getcwd,
  [SYS_unlink]      sys_unlink,
  [SYS_trace]       sys_trace,
  [SYS_sysinfo]     sys_sysinfo,
  [SYS_rename]      sys_rename,
  [SYS_execve]      sys_execve,
  [SYS_mount]      sys_mount,
  [SYS_umount]      sys_umount,
};

static char *sysnames[] = {
  [SYS_fork]        "fork",
  [SYS_exit]        "exit",
  [SYS_wait]        "wait",
  [SYS_pipe]        "pipe",
  [SYS_read]        "read",
  [SYS_kill]        "kill",
  [SYS_exec]        "exec",
  [SYS_fstat]       "fstat",
  [SYS_chdir]       "chdir",
  [SYS_dup]         "dup",
  [SYS_getpid]      "getpid",
  [SYS_sbrk]        "sbrk",
  [SYS_sleep]       "sleep",
  [SYS_uptime]      "uptime",
  [SYS_open]        "open",
  [SYS_write]       "write",
  [SYS_mkdir]       "mkdir",
  [SYS_close]       "close",
  [SYS_test_proc]   "test_proc",
  [SYS_dev]         "dev",
  [SYS_readdir]     "readdir",
  [SYS_getcwd]      "getcwd",
  [SYS_unlink]      "unlink",
  [SYS_trace]       "trace",
  [SYS_sysinfo]     "sysinfo",
  [SYS_rename]      "rename",
  [SYS_execve]      "execve",
  [SYS_mount]       "mount",
  [SYS_umount]      "umount",
};

void
syscall(void)
{
  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // trace
    int trace = p->tmask & (1 << (num - 1));
    if (trace) {
      printf("pid %d: %s(", p->pid, sysnames[num]);
    }
    p->trapframe->a0 = syscalls[num]();
    if (trace) {
      printf(") -> %d\n", p->trapframe->a0);
    }
    
  } else {
    printf("pid %d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}

uint64 
sys_test_proc(void) {
    int n;
    argint(0, &n);
    printf("hello world from proc %d, hart %d, arg %d\n", myproc()->pid, r_tp(), n);
    return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  // struct proc *p = myproc();

  if (argaddr(0, &addr) < 0) {
    return -1;
  }

  struct sysinfo info;
  info.freemem = idlepages() << PGSHIFT;
  info.nproc = procnum();

  // if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
  if (copyout2(addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }

  return 0;
}