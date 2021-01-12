#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "xv6-user/user.h"

char *argv[] = { "_test", 0 };
int main()
{
  int pid, wpid;
  
  for(;;){
    pid = fork();
    if(pid < 0){
      exit(1);
    }
    if(pid == 0){
      exec("_test", argv);
      exit(1);
      test_proc();
    }

    for(;;){
      // this call to wait() returns if the shell exits,
      // or if a parentless process exits.
      wpid = wait((int *) 0);
      if(wpid == pid){
        // the shell exited; restart it.
        // break;
      } else if(wpid < 0){
        exit(1);
      } else {
        // it was a parentless process; do nothing.
      }
    }
  }
    return 0;
}