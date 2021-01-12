//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//


#include "include/types.h"
#include "include/riscv.h"
#include "include/defs.h"
#include "include/param.h"
#include "include/stat.h"
#include "include/spinlock.h"
#include "include/proc.h"
#include "include/fs.h"
#include "include/sleeplock.h"
#include "include/file.h"
#include "include/fcntl.h"
#include "include/fat32.h"


// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
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

// // Create the path new as a link to the same inode as old.
// uint64
// sys_link(void)
// {
//   char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
//   struct inode *dp, *ip;

//   if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
//     return -1;

//   begin_op();
//   if((ip = namei(old)) == 0){
//     end_op();
//     return -1;
//   }

//   ilock(ip);
//   if(ip->type == T_DIR){
//     iunlockput(ip);
//     end_op();
//     return -1;
//   }

//   ip->nlink++;
//   iupdate(ip);
//   iunlock(ip);

//   if((dp = nameiparent(new, name)) == 0)
//     goto bad;
//   ilock(dp);
//   if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
//     iunlockput(dp);
//     goto bad;
//   }
//   iunlockput(dp);
//   iput(ip);

//   end_op();

//   return 0;

// bad:
//   ilock(ip);
//   ip->nlink--;
//   iupdate(ip);
//   iunlockput(ip);
//   end_op();
//   return -1;
// }

// // Is the directory dp empty except for "." and ".." ?
// static int
// isdirempty(struct dir_entry *dp)
// {
//   // int off;
//   // struct dirent de;

//   // for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
//   //   if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//   //     panic("isdirempty: readi");
//   //   if(de.inum != 0)
//   //     return 0;
//   // }
//   return 1;
// }

// uint64
// sys_unlink(void)
// {
//   struct inode *ip, *dp;
//   struct dirent de;
//   char name[DIRSIZ], path[MAXPATH];
//   uint off;

//   if(argstr(0, path, MAXPATH) < 0)
//     return -1;

//   begin_op();
//   if((dp = nameiparent(path, name)) == 0){
//     end_op();
//     return -1;
//   }

//   ilock(dp);

//   // Cannot unlink "." or "..".
//   if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
//     goto bad;

//   if((ip = dirlookup(dp, name, &off)) == 0)
//     goto bad;
//   ilock(ip);

//   if(ip->nlink < 1)
//     panic("unlink: nlink < 1");
//   if(ip->type == T_DIR && !isdirempty(ip)){
//     iunlockput(ip);
//     goto bad;
//   }

//   memset(&de, 0, sizeof(de));
//   if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
//     panic("unlink: writei");
//   if(ip->type == T_DIR){
//     dp->nlink--;
//     iupdate(dp);
//   }
//   iunlockput(dp);

//   ip->nlink--;
//   iupdate(ip);
//   iunlockput(ip);

//   end_op();

//   return 0;

// bad:
//   iunlockput(dp);
//   end_op();
//   return -1;
// }

static struct dir_entry*
create(char *path, short type, short major, short minor)
{
  struct dir_entry *ep, *dp;
  char name[FAT32_MAX_FILENAME];

  if((dp = get_parent(path, name)) == 0)
    return 0;

  elock(dp);

  if((ep = get_entry(path)) != 0){
    eunlock(dp);
    eput(dp);
    elock(ep);
    if(type == T_FILE && (ep->type == T_FILE || ep->type == T_DEVICE))
      return ep;
    eunlock(ep);
    eput(ep);
    return 0;
  }

  if((ep = ealloc(dp, name)) == 0)
    panic("create: ialloc");

  elock(ep);
  // ip->major = major;
  // ip->minor = minor;
  // ip->nlink = 1;
  // iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    // dp->nlink++;  // for ".."
    // iupdate(dp);
    // // No ip->nlink++ for ".": avoid cyclic ref count.
    // if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
    //   panic("create dots");
    ep->attribute = ATTR_DIRECTORY;
    // Create . and .. entries.
  }

  // if(dirlink(dp, name, ip->inum) < 0)
  //   panic("create: dirlink");

  eunlock(dp);
  eput(dp);

  return ep;
}

uint64
sys_open(void)
{
  char path[FAT32_MAX_PATH];
  int fd, omode;
  struct file *f;
  struct dir_entry *ep;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  // begin_op();

  if(omode & O_CREATE){
    ep = create(path, T_FILE, 0, 0);
    if(ep == 0){
      // end_op();
      return -1;
    }
  } else {
    if((ep = get_entry(path)) == 0){
      // end_op();
      return -1;
    }
    elock(ep);
    if(ep->attribute == ATTR_DIRECTORY && omode != O_RDONLY){
      eunlock(ep);
      eput(ep);
      // end_op();
      return -1;
    }
  }

  if(ep->type == T_DEVICE /* && (ep->major < 0 || ep->major >= NDEV) */){
    eunlock(ep);
    eput(ep);
    // end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    eunlock(ep);
    eput(ep);
    // end_op();
    return -1;
  }

  if(ep->type == T_DEVICE){
    f->type = FD_DEVICE;
    // f->major = ep->major;
  } else {
    f->type = FD_ENTRY;
    f->off = 0;
  }
  f->ep = ep;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  // if((omode & O_TRUNC) && ep->type == T_FILE){
  //   itrunc(ip);
  // }

  eunlock(ep);
  // end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[FAT32_MAX_PATH];
  struct dir_entry *ep;

  // begin_op();
  if(argstr(0, path, FAT32_MAX_PATH) < 0 || (ep = create(path, T_DIR, 0, 0)) == 0){
    // end_op();
    return -1;
  }
  eunlock(ep);
  eput(ep);
  // end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct dir_entry *ep;
  char path[FAT32_MAX_PATH];
  int major, minor;

  // begin_op();
  if((argstr(0, path, FAT32_MAX_PATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ep = create(path, T_DEVICE, major, minor)) == 0){
    // end_op();
    return -1;
  }
  eunlock(ep);
  eput(ep);
  // end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct dir_entry *ep;
  struct proc *p = myproc();
  
  // begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ep = get_entry(path)) == 0){
    // end_op();
    return -1;
  }
  elock(ep);
  if(ep->attribute != ATTR_DIRECTORY){
    eunlock(ep);
    eput(ep);
    // end_op();
    return -1;
  }
  eunlock(ep);
  eput(p->cwd);
  // end_op();
  p->cwd = ep;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
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
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}
