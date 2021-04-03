#ifndef __SYSINFO_H
#define __SYSINFO_H

#include "types.h"

struct sysinfo {
  uint64 freemem;   // amount of free memory (bytes)
  uint64 nproc;     // number of process
};


#endif
