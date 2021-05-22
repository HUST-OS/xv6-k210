#include "kernel/include/types.h"
#include "kernel/include/stat.h"
#include "kernel/include/fcntl.h"
#include "kernel/include/sysinfo.h"

// struct stat;
struct rtcdate;
struct sysinfo;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int fd, const void *buf, int len);
int read(int fd, void *buf, int len);
int close(int fd);
int kill(int pid);
int exec(char*, char**);
int execve(char *name, char *argv[], char *envp[]);
int open(const char *filename, int mode);
int fstat(int fd, struct kstat*);
int mkdir(const char *dirname);
int chdir(const char *dirname);
int dup(int fd);
int getpid(void);
char* sbrk(int size);
int sleep(int ticks);
int uptime(void);
int test_proc(int);
int dev(int, short, short);
int getcwd(char *buf, uint size);
int remove(char *filename);
int trace(int mask);
int sysinfo(struct sysinfo *);
int rename(char *old, char *new);
int mount(char *dev, char *dir, char *fstype, uint64 flag, void *data);
int getdents(int fd, struct dirent *buf, unsigned long len);
int umount(char *dir, uint64 flag);

// ulib.c
int stat(const char*, struct kstat*);
char* strcpy(char*, const char*);
char* strcat(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
void putc(int ch, int fd);
void putchar(int ch);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
