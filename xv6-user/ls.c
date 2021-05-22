#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

// char*
// fmtname(char *name)
// {
//   static char buf[DIRENT_MAX_NAME+1];
//   int len = strlen(name);

//   // Return blank-padded name.
//   if(len >= DIRENT_MAX_NAME)
//     return name;
//   memmove(buf, name, len);
//   memset(buf + len, ' ', DIRENT_MAX_NAME - len);
//   buf[DIRENT_MAX_NAME] = '\0';
//   return buf;
// }

char *getname(char *path)
{
  char *p = path + strlen(path) - 1;
  while (p > path && *p == '/')
    p--;

  p[1] = '\0';
  while (p > path && *p != '/')
    p--;

  return p + 1;
}

void
ls(char *path, int list)
{
  int fd;
  struct kstat st;
  char *types[] = {
    [T_DIR]   "DIR ",
    [T_FILE]  "FILE",
  };
  char sizes[] = "BKMGT";

  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  struct dirent dent;
  char buf[260 + 255 + 2];
  if (st.mode == T_DIR){
    int len = strlen(path);
    if (len >= 260) {
      fprintf(2, "ls: path \"%s\" overflow\n", path);
      close(fd);
      return;
    }
    strcpy(buf, path);
    buf[len] = '/';
    char *p = buf + len + 1;
    while(getdents(fd, &dent, sizeof(dent)) > 0){
      strcpy(p, dent.name);
      if (stat(buf, &st) < 0) {
        fprintf(2, "ls: cannot stat %s\n", dent.name);
      } else {
        int szno = 0;
        while (st.size >= 1024) {
          st.size /= 1024;
          szno++;
        }
        printf("%x\t%d\t%s\t%l%c\t%s\n", st.dev, st.ino, types[st.mode], st.size, sizes[szno], dent.name);
      }
    }
  } else {
    int szno = 0;
    while (st.size >= 1024) {
      st.size /= 1024;
      szno++;
    }
    printf("%x\t%d\t%s\t%l%c\t%s\n",
          st.dev, st.ino, types[st.mode], st.size, sizes[szno], getname(path));
  }
  close(fd);
}

int
main(int argc, char *argv[])
{
  int i;

  if(argc < 2){
    ls(".", 0);
    exit(0);
  }
  for(i=1; i<argc; i++)
    ls(argv[i], 0);
  exit(0);
}
