#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdint.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int open(const char* pathname, int flags);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int unlink(const char* pathname);
int rmdir(const char* pathname);
int mkdir(const char* pathname, uint32_t mode);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
unsigned int sleep(unsigned int seconds);
unsigned int usleep(unsigned int usecs);
int execve(const char* filename, char* const argv[], char* const envp[]);
int fork(void);
void _exit(int status);
pid_t getpid(void);
pid_t getppid(void);

/* Flags for open */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0100
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

typedef int pid_t;
typedef int ssize_t;

#endif
