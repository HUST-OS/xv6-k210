#ifndef __SLEEPLOCK_H
#define __SLEEPLOCK_H

#include "include/spinlock.h"
#include "include/types.h"


// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

#endif
