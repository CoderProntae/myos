#ifndef POSIX_H
#define POSIX_H

#include <stdint.h>
#include "libc.h"

/* ======== ERRNO ======== */
#define EPERM        1
#define ENOENT       2
#define ESRCH        3
#define EINTR        4
#define EIO          5
#define ENOMEM      12
#define EACCES      13
#define EFAULT      14
#define EEXIST      17
#define ENOTDIR     20
#define EISDIR      21
#define EINVAL      22
#define EMFILE      24
#define ENOSPC      28
#define EPIPE       32
#define ENOSYS      38
#define ENOTEMPTY   39
#define ECONNREFUSED 111
#define ETIMEDOUT   110
#define EHOSTUNREACH 113

extern int posix_errno;
#define errno posix_errno

const char* strerror(int errnum);

/* ======== PROCESS ======== */
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long off_t;
typedef int mode_t;

pid_t getpid(void);
pid_t getppid(void);
uid_t getuid(void);
gid_t getgid(void);
pid_t fork(void);
int   execve(const char* path, char* const argv[], char* const envp[]);
int   execv(const char* path, char* const argv[]);
pid_t waitpid(pid_t pid, int* status, int options);
void  _exit(int status);

/* ======== SIGNALS ======== */
#define SIGHUP    1
#define SIGINT    2
#define SIGQUIT   3
#define SIGILL    4
#define SIGABRT   6
#define SIGFPE    8
#define SIGKILL   9
#define SIGSEGV  11
#define SIGPIPE  13
#define SIGALRM  14
#define SIGTERM  15
#define SIGCHLD  17
#define SIGCONT  18
#define SIGSTOP  19
#define SIG_MAX  32

#define SIG_DFL ((sig_handler_t)0)
#define SIG_IGN ((sig_handler_t)1)
#define SIG_ERR ((sig_handler_t)-1)

typedef void (*sig_handler_t)(int);

sig_handler_t signal(int signum, sig_handler_t handler);
int kill(pid_t pid, int sig);
int raise(int sig);

/* ======== ENVIRONMENT ======== */
#define ENV_MAX 32
#define ENV_VAL_MAX 128

char* getenv(const char* name);
int   setenv(const char* name, const char* value, int overwrite);
int   unsetenv(const char* name);
void  env_init(void);

/* ======== PIPE ======== */
#define PIPE_BUF_SIZE 512
#define PIPE_MAX 4

typedef struct {
    uint8_t buf[PIPE_BUF_SIZE];
    int     read_pos;
    int     write_pos;
    int     count;
    int     read_open;
    int     write_open;
    int     active;
} pipe_t;

int pipe(int pipefd[2]);
ssize_t pipe_read(int fd, void* buf, size_t count);
ssize_t pipe_write(int fd, const void* buf, size_t count);

/* ======== DIRECTORY ======== */
typedef struct {
    char name[64];
    int  type;     /* 0=file, 1=dir */
    int  size;
    int  node_id;
} dirent_t;

typedef struct {
    int parent;
    int pos;
    int active;
} DIR_t;

DIR_t*     opendir(const char* path);
dirent_t*  readdir(DIR_t* dir);
int        closedir(DIR_t* dir);

/* ======== TIME ======== */
typedef long time_t;

typedef struct {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
} tm_t;

time_t time_now(time_t* t);
tm_t*  localtime(const time_t* t);
int    gettimeofday_sec(void);

/* ======== MISC ======== */
int     isatty(int fd);
char*   getcwd(char* buf, size_t size);
int     chdir(const char* path);
int     access(const char* path, int mode);
unsigned int sleep_sec(unsigned int seconds);
int     usleep(uint32_t usec);
long    sysconf(int name);
void    abort(void);

/* sysconf names */
#define _SC_PAGESIZE      1
#define _SC_NPROCESSORS   2
#define _SC_PHYS_PAGES    3

/* access modes */
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1

/* Posix init */
void posix_init(void);

#endif
