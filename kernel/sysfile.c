//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//


#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/sleeplock.h"
#include "include/file.h"
#include "include/pipe.h"
#include "include/fcntl.h"
#include "include/fs.h"
#include "include/syscall.h"
#include "include/string.h"
#include "include/printf.h"
#include "include/vm.h"


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == NULL)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, path, MAXPATH) < 0 || argint(1, &omode) < 0)
    return -1;

  if(omode & O_CREATE){
    ip = create(path, T_FILE);
    if(ip == NULL){
      return -1;
    }
  } else {
    if((ip = namei(path)) == NULL){
      return -1;
    }
    ilock(ip);
    if (ip->mode == T_DIR && omode != O_RDONLY) {
      iunlockput(ip);
      return -1;
    }
  }

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if (f) {
      fileclose(f);
    }
    iunlockput(ip);
    return -1;
  }

  if (ip->mode != T_DIR && (omode & O_TRUNC)) {
    ip->op->truncate(ip);
  }

  f->type = FD_INODE;
  f->off = (omode & O_APPEND) ? ip->size : 0;
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  iunlock(ip);

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR)) == 0){
    return -1;
  }
  iunlockput(ip);
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == NULL){
    return -1;
  }
  if (ip->mode != T_DIR) {
    iput(ip);
    return -1;
  }
  iput(p->cwd);
  p->cwd = ip;
  return 0;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  // if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
  //    copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
  if(copyout2(fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout2(fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

// To open console device.
uint64
sys_dev(void)
{
  int fd, omode;
  int major, minor;
  struct file *f;

  if(argint(0, &omode) < 0 || argint(1, &major) < 0 || argint(2, &minor) < 0){
    return -1;
  }

  if(omode & O_CREATE){
    panic("dev file on FAT");
  }

  if(major < 0 || major >= NDEV)
    return -1;

  if((f = filealloc()) == NULL || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    return -1;
  }

  f->type = FD_DEVICE;
  f->off = 0;
  f->ip = 0;
  f->major = major;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  return fd;
}

// To support ls command
uint64
sys_readdir(void)
{
  struct file *f;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argaddr(1, &p) < 0)
    return -1;
  return filereaddir(f, p);
}

// get absolute cwd string
uint64
sys_getcwd(void)
{
  uint64 addr;
  uint64 size;
  if (argaddr(0, &addr) < 0 || argint(1, (int*)&size) < 0)
    return -1;

  if (size < 2)
    return -1;

  char buf[MAXPATH];

  int max = MAXPATH < size ? MAXPATH : size;
  if (namepath(myproc()->cwd, buf, max) < 0)
    return -1;

  if (copyout2(addr, buf, max) < 0)
    return -1;

  return 0;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  struct stat st;
  int off = 0, ret;
  while (1) {
    ret = dp->fop->readdir(dp, &st, off);
    if (ret < 0)
      return -1;
    else if (ret == 0)
      return 1;
    else if ((st.name[0] == '.' && st.name[1] == '\0') ||     // skip the "." and ".."
             (st.name[0] == '.' && st.name[1] == '.' && st.name[2] == '\0'))
    {
      off += ret;
    }
    else
      return 0;
  }
}

uint64
sys_remove(void)
{
  char path[MAXPATH];
  struct inode *ip;
  int len;
  if((len = argstr(0, path, MAXPATH)) <= 0)
    return -1;

  char *s = path + len - 1;
  while (s >= path && *s == '/') {
    s--;
  }
  if (s >= path && *s == '.' && (s == path || *--s == '/')) {
    return -1;
  }
  
  if((ip = namei(path)) == NULL){
    return -1;
  }
  ilock(ip);
  if (ip->mode == T_DIR && isdirempty(ip) != 1) {
      iunlockput(ip);
      return -1;
  }
  int ret = unlink(ip);
  iunlockput(ip);

  return ret;
}

// Must hold too many locks at a time! It's possible to raise a deadlock.
// Because this op takes some steps, we can't promise
uint64
sys_rename(void)
{
//   char old[MAXPATH], new[MAXPATH];
//   if (argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0) {
      return -1;
//   }

//   struct inode *src = NULL, *dst = NULL, *pdst = NULL;
//   int srclock = 0;
//   char *name;
//   if ((src = ename(old)) == NULL || (pdst = enameparent(new, old)) == NULL
//       || (name = formatname(old)) == NULL) {
//     goto fail;          // src doesn't exist || dst parent doesn't exist || illegal new name
//   }
//   for (struct inode *ep = pdst; ep != NULL; ep = ep->parent) {
//     if (ep == src) {    // In what universe can we move a directory into its child?
//       goto fail;
//     }
//   }

//   uint off;
//   elock(src);     // must hold child's lock before acquiring parent's, because we do so in other similar cases
//   srclock = 1;
//   elock(pdst);
//   dst = dirlookup(pdst, name, &off);
//   if (dst != NULL) {
//     eunlock(pdst);
//     if (src == dst) {
//       goto fail;
//     } else if (src->attribute & dst->attribute & ATTR_DIRECTORY) {
//       elock(dst);
//       if (!isdirempty(dst)) {    // it's ok to overwrite an empty dir
//         eunlock(dst);
//         goto fail;
//       }
//       elock(pdst);
//       eremove(dst);
//       eunlock(dst);
//       eput(dst);
//       dst = NULL;
//     } else {                    // src is not a dir || dst exists and is not an dir
//       goto fail;
//     }
//   }

//   struct inode *psrc = src->parent;  // src must not be root, or it won't pass the for-loop test
//   memmove(src->filename, name, FAT32_MAX_FILENAME);
//   if (emake(pdst, src, off) < 0) {
//     eunlock(pdst);
//     goto fail;
//   }
//   if (psrc != pdst) {
//     elock(psrc);
//   }
//   eremove(src);
//   ereparent(pdst, src);
//   src->off = off;
//   src->valid = 1;
//   if (psrc != pdst) {
//     eunlock(psrc);
//   }
//   eunlock(pdst);
//   eunlock(src);

//   eput(psrc);   // because src no longer points to psrc
//   eput(pdst);
//   eput(src);

//   return 0;

// fail:
//   if (srclock)
//     eunlock(src);
//   if (dst)
//     eput(dst);
//   if (pdst)
//     eput(pdst);
//   if (src)
//     eput(src);
//  return -1;
}
