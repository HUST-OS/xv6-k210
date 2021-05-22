#ifndef __STAT_H
#define __STAT_H

#define T_DIR     1   // Directory
#define T_FILE    2   // File
#define T_DEVICE  3   // Device

#define DIRENT_MAX_NAME 255

struct kstat {
  uint32    dev;
  uint64    ino;
  uint16    mode;
  uint32    nlink;
  uint32    uid;
  uint32    gid;
  uint32    rdev;
  uint64    __pad;
  uint64    size;
  uint32    blksize;
  int       __pad2;
  uint64    blocks;
  long      atime_sec;
  long      atime_nsec;
  long      mtime_sec;
  long      mtime_nsec;
  long      ctime_sec;
  long      ctime_nsec;
  uint32    __unused[2];
};

struct dirent {
  uint64    ino;
  int64     off;
  uint16    reclen; // Length of name
  uint8     type;
  char      name[DIRENT_MAX_NAME + 1];
};

// struct stat {
//   int dev;     // File system's disk device
//   uint ino;    // Inode number
//   short type;  // Type of file
//   short nlink; // Number of links to file
//   uint64 size; // Size of file in bytes
// };

#endif