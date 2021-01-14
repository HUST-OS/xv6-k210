#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"
// #include "kernel/include/fs.h"
#include "kernel/include/fat32.h"

#define FILENAME_LENGTH 32

char*
fmtname(char *path)
{
  static char buf[FILENAME_LENGTH+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if(strlen(p) >= FILENAME_LENGTH)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), ' ', FILENAME_LENGTH-strlen(p));
  return buf;
}

void
ls(char *path)
{
  char buf[512], *p;
  int fd;
  // struct dirent de;
  struct stat st;

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.attribute == ATTR_DIRECTORY){

    if(strlen(path) + 1 + FILENAME_LENGTH + 1 > sizeof buf){
      printf("ls: path too long\n");
    }
    strcpy(buf, path);
    p = buf+strlen(buf);
    *p++ = '/';

// CAN'T WORK NOW

    while(read(fd, &de, sizeof(de)) == sizeof(de)){
      // if(de.inum == 0)
      //   continue;
      // memmove(p, de.name, FILENAME_LENGTH);
      // p[DIRSIZ] = 0;
      if(stat(buf, &st) < 0){
        printf("ls: cannot stat %s\n", buf);
        continue;
      }
      printf("%s %d %d\n", fmtname(buf), st.attribute, st.size);
    }
  } else {
    printf("%s %d %l\n", fmtname(path), st.attribute, st.size);
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".");
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i]);
  exit(0);
}
