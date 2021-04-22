#include "user.h"

char *myenvp[] = {
    "PATH=/bin",
    "env1=xxx",
    "env2=yyy",
    "env3=zzz",
};

int main(int argc, char *argv[], char *envp[])
{
    printf("argc: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("arg%d: [%s]\n", i, argv[i]);
    }
    for (int i = 0; envp[i]; i++) {
        printf("env%d: [%s]\n", i, envp[i]);
    }
    int fk = 0;
    if (argc > 1) {
        fk = atoi(argv[1]);
    }
    if (fk && fork() == 0) {
        strcpy(argv[1], "0");
        execve("test", argv, myenvp);
    }
    exit(0);
}
