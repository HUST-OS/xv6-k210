#include "kernel/include/param.h"
#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

char *getenv(char *envp[], char *envvar)
{
  if (envp == NULL) {
    return NULL;
  }
  for (int i = 0; envp[i]; i++) {
    int j = 0; 
    while (envvar[j] == envp[i][j]) {
      j++;
    }
    if (envvar[j] == 0 && envp[i][j] == '=') {
      return &envp[i][j + 1];
    }
  }
  return NULL;
}

int
main(int argc, char *argv[], char *envp[])
{
  int i;
  char *nargv[MAXARG];

  if(argc < 3){
    fprintf(2, "usage: %s MASK COMMAND\n", argv[0]);
    exit(1);
  }

  if (trace(atoi(argv[1])) < 0) {
    fprintf(2, "%s: strace failed\n", argv[0]);
    exit(1);
  }
  
  for(i = 2; i < argc && i < MAXARG; i++){
    nargv[i-2] = argv[i];
  }
  execve(nargv[0], nargv, envp);
  char buf[128];
  char *env = getenv(envp, "PATH");
  if (env != NULL) {
    while (*env) {
      int i;
      for (i = 0; i < 128; i++) {
        buf[i] = *env++;
        if (buf[i] == ';' || buf[i] == '\0') {
          buf[i] = '/';
          break;
        }
      }
      char *argp = argv[2];
      for (i++; i < 128; i++) {
        if ((buf[i] = *argp++) == '\0') {
          break;
        }
      }
      if (i < 128) {
        execve(buf, nargv, envp);
      }
    }
  }
  printf("strace: exec %s fail\n", nargv[0]);
  exit(0);
}
