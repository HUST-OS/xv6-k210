//
// Support functions for system calls that involve file descriptors.
//


#include "include/types.h"
#include "include/riscv.h"
#include "include/param.h"
#include "include/spinlock.h"
#include "include/sleeplock.h"
#include "include/fs.h"
#include "include/file.h"
#include "include/pipe.h"
#include "include/stat.h"
#include "include/proc.h"
#include "include/printf.h"
#include "include/string.h"
#include "include/vm.h"
#include "include/kmalloc.h"

struct devsw devsw[NDEV];
// struct {
//   struct spinlock lock;
//   struct file file[NFILE];
// } ftable;

// void
// fileinit(void)
// {
//   initlock(&ftable.lock, "ftable");
//   struct file *f;
//   for(f = ftable.file; f < ftable.file + NFILE; f++){
//     memset(f, 0, sizeof(struct file));
//   }
//   #ifdef DEBUG
//   printf("fileinit\n");
//   #endif
// }

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;
  f = (struct file *)kmalloc(sizeof(struct file));
  if (f == NULL) {
    return NULL;
  }

  f->ref = 1;
  return f;
}
// struct file*
// filealloc(void)
// {
//   struct file *f;

//   acquire(&ftable.lock);
//   for(f = ftable.file; f < ftable.file + NFILE; f++){
//     if(f->ref == 0){
//       f->ref = 1;
//       release(&ftable.lock);
//       return f;
//     }
//   }
//   release(&ftable.lock);
//   return NULL;
// }

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  // acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  // release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  // struct file ff;

  // acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    // release(&ftable.lock);
    return;
  }
  // ff = *f;
  // f->ref = 0;
  // f->type = FD_NONE;
  // release(&ftable.lock);

  if(f->type == FD_PIPE){
    pipeclose(f->pipe, f->writable);
  } else if (f->type == FD_INODE) {
    iput(f->ip);
  } else if (f->type == FD_DEVICE) {

  }
  kfree(f);
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  // struct proc *p = myproc();
  struct kstat st;
  
  if(f->type == FD_INODE){
    ilock(f->ip);
    f->ip->op->getattr(f->ip, &st);
    iunlock(f->ip);
    // if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
    if(copyout2(addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;
  struct inode *ip;

  if(f->readable == 0)
    return -1;

  switch (f->type) {
    case FD_PIPE:
        r = piperead(f->pipe, addr, n);
        break;
    case FD_DEVICE:
        if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
          return -1;
        r = devsw[f->major].read(1, addr, n);
        break;
    case FD_INODE:
        ip = f->ip;
        ilock(ip);
          if((r = ip->fop->read(ip, 1, addr, f->off, n)) > 0)
            f->off += r;
        iunlock(ip);
        break;
    default:
      panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    struct inode *ip = f->ip;
    ilock(ip);
    if (ip->fop->write(ip, 1, addr, f->off, n) == n) {
      ret = n;
      f->off += n;
    } else {
      ret = -1;
    }
    iunlock(ip);
  } else {
    panic("filewrite");
  }

  return ret;
}

// Read from dir f.
// addr is a user virtual address.
int
filereaddir(struct file *f, uint64 addr, uint64 len)
{
  // struct proc *p = myproc();
  if (f->type != FD_INODE || f->readable == 0)
    return -1;

  struct dirent dent;
  struct inode *ip = f->ip;

  if(ip->mode != T_DIR)
    return -1;

  int ret;
  ilock(ip);
  ret = ip->fop->readdir(ip, &dent, f->off);
  iunlock(ip);
  if (ret <= 0) // 0 is end, -1 is err
    return ret;

  f->off += ret;
  // if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
  if(copyout2(addr, (char *)&dent, sizeof(dent) > len ? len : sizeof(dent)) < 0)
    return -1;

  return ret;
}