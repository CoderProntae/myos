#include "posix.h"
#include "string.h"
#include "heap.h"
#include "vfs.h"
#include "task.h"
#include "io.h"

/* ======== ERRNO ======== */

int posix_errno = 0;

const char* strerror(int errnum) {
    switch (errnum) {
        case 0:            return "Basarili";
        case EPERM:        return "Izin yok";
        case ENOENT:       return "Dosya bulunamadi";
        case ESRCH:        return "Islem bulunamadi";
        case EINTR:        return "Kesilen cagri";
        case EIO:          return "G/C hatasi";
        case ENOMEM:       return "Bellek yetersiz";
        case EACCES:       return "Erisim reddedildi";
        case EEXIST:       return "Dosya zaten var";
        case ENOTDIR:      return "Dizin degil";
        case EISDIR:       return "Dizin";
        case EINVAL:       return "Gecersiz arguman";
        case EMFILE:       return "Cok fazla acik dosya";
        case ENOSPC:       return "Disk dolu";
        case EPIPE:        return "Kirik boru";
        case ENOSYS:       return "Desteklenmeyen islem";
        case ENOTEMPTY:    return "Dizin bos degil";
        case ECONNREFUSED: return "Baglanti reddedildi";
        case ETIMEDOUT:    return "Zaman asimi";
        case EHOSTUNREACH: return "Erisilemeyen host";
        default:           return "Bilinmeyen hata";
    }
}

/* ======== PROCESS ======== */

static pid_t current_pid = 1;
static pid_t parent_pid = 0;

pid_t getpid(void) { return current_pid; }
pid_t getppid(void) { return parent_pid; }
uid_t getuid(void) { return 0; } /* root */
gid_t getgid(void) { return 0; }

pid_t fork(void) {
    /* Gercek fork yok — simule et */
    errno = ENOSYS;
    return -1;
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    (void)argv;
    (void)envp;

    int node = vfs_find_path(path);
    if (node < 0) { errno = ENOENT; return -1; }

    vfs_node_t* n = vfs_get_node(node);
    if (!n || !n->data || n->size < 4) { errno = ENOENT; return -1; }

    /* ELF/MyOS magic kontrol */
    uint32_t magic = *(uint32_t*)n->data;
    if (magic != 0x464C457F && magic != 0x4D594F53) {
        errno = EINVAL;
        return -1;
    }

    /* Task olarak calistir — basit versiyon */
    errno = ENOSYS;
    return -1;
}

int execv(const char* path, char* const argv[]) {
    return execve(path, argv, 0);
}

pid_t waitpid(pid_t pid, int* status, int options) {
    (void)pid;
    (void)options;
    if (status) *status = 0;
    errno = ENOSYS;
    return -1;
}

void _exit(int status) {
    (void)status;
    task_exit();
    while (1);
}

/* ======== SIGNALS ======== */

static sig_handler_t sig_handlers[SIG_MAX];

sig_handler_t signal(int signum, sig_handler_t handler) {
    if (signum < 0 || signum >= SIG_MAX) {
        errno = EINVAL;
        return SIG_ERR;
    }
    sig_handler_t old = sig_handlers[signum];
    sig_handlers[signum] = handler;
    return old;
}

int kill(pid_t pid, int sig) {
    if (pid <= 0 || sig < 0 || sig >= SIG_MAX) {
        errno = EINVAL;
        return -1;
    }
    /* Kendi pid'imiz mi? */
    if (pid == current_pid) {
        return raise(sig);
    }
    /* Baska process yok — task kill dene */
    if (pid < MAX_TASKS) {
        if (sig == SIGKILL || sig == SIGTERM) {
            task_kill(pid);
            return 0;
        }
    }
    errno = ESRCH;
    return -1;
}

int raise(int sig) {
    if (sig < 0 || sig >= SIG_MAX) { errno = EINVAL; return -1; }

    if (sig == SIGKILL) {
        _exit(128 + sig);
        return 0;
    }

    sig_handler_t handler = sig_handlers[sig];
    if (handler == SIG_DFL) {
        /* Varsayilan davranis */
        switch (sig) {
            case SIGTERM:
            case SIGINT:
            case SIGQUIT:
            case SIGABRT:
                _exit(128 + sig);
                break;
            case SIGCHLD:
            case SIGCONT:
                break; /* Yoksay */
            default:
                break;
        }
    } else if (handler != SIG_IGN) {
        handler(sig);
    }
    return 0;
}

/* ======== ENVIRONMENT ======== */

static char env_names[ENV_MAX][64];
static char env_values[ENV_MAX][ENV_VAL_MAX];
static int  env_count = 0;

void env_init(void) {
    env_count = 0;

    /* Varsayilan ortam degiskenleri */
    setenv("HOME", "/home", 1);
    setenv("PATH", "/bin", 1);
    setenv("USER", "root", 1);
    setenv("SHELL", "/bin/terminal", 1);
    setenv("TERM", "myos-term", 1);
    setenv("LANG", "tr_TR.UTF-8", 1);
    setenv("HOSTNAME", "myos", 1);
    setenv("OS", "MyOS", 1);
    setenv("VERSION", "0.3.0", 1);
    setenv("ARCH", "i686", 1);
    setenv("PWD", "/home", 1);
}

char* getenv(const char* name) {
    for (int i = 0; i < env_count; i++) {
        if (k_strcmp(env_names[i], name) == 0)
            return env_values[i];
    }
    return NULL;
}

int setenv(const char* name, const char* value, int overwrite) {
    if (!name || !value) { errno = EINVAL; return -1; }

    /* Mevcut mu? */
    for (int i = 0; i < env_count; i++) {
        if (k_strcmp(env_names[i], name) == 0) {
            if (overwrite) {
                k_strncmp(value, env_values[i], ENV_VAL_MAX - 1); /* dummy call */
                strncpy(env_values[i], value, ENV_VAL_MAX - 1);
                env_values[i][ENV_VAL_MAX - 1] = '\0';
            }
            return 0;
        }
    }

    /* Yeni ekle */
    if (env_count >= ENV_MAX) { errno = ENOMEM; return -1; }
    strncpy(env_names[env_count], name, 63);
    env_names[env_count][63] = '\0';
    strncpy(env_values[env_count], value, ENV_VAL_MAX - 1);
    env_values[env_count][ENV_VAL_MAX - 1] = '\0';
    env_count++;
    return 0;
}

int unsetenv(const char* name) {
    for (int i = 0; i < env_count; i++) {
        if (k_strcmp(env_names[i], name) == 0) {
            /* Son elemani buraya tasI */
            env_count--;
            if (i < env_count) {
                strcpy(env_names[i], env_names[env_count]);
                strcpy(env_values[i], env_values[env_count]);
            }
            return 0;
        }
    }
    errno = ENOENT;
    return -1;
}

/* ======== PIPE ======== */

static pipe_t pipes[PIPE_MAX];

int pipe(int pipefd[2]) {
    for (int i = 0; i < PIPE_MAX; i++) {
        if (!pipes[i].active) {
            k_memset(&pipes[i], 0, sizeof(pipe_t));
            pipes[i].active = 1;
            pipes[i].read_open = 1;
            pipes[i].write_open = 1;
            /* fd numaralari: 100+i*2 (read), 100+i*2+1 (write) */
            pipefd[0] = 100 + i * 2;
            pipefd[1] = 100 + i * 2 + 1;
            return 0;
        }
    }
    errno = EMFILE;
    return -1;
}

ssize_t pipe_read(int fd, void* buf, size_t count) {
    int idx = (fd - 100) / 2;
    if (idx < 0 || idx >= PIPE_MAX || !pipes[idx].active) {
        errno = EINVAL;
        return -1;
    }
    pipe_t* p = &pipes[idx];
    if (p->count == 0) return 0;

    uint8_t* dst = (uint8_t*)buf;
    size_t read_count = 0;
    while (read_count < count && p->count > 0) {
        dst[read_count++] = p->buf[p->read_pos];
        p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
        p->count--;
    }
    return (ssize_t)read_count;
}

ssize_t pipe_write(int fd, const void* buf, size_t count) {
    int idx = (fd - 100) / 2;
    if (idx < 0 || idx >= PIPE_MAX || !pipes[idx].active) {
        errno = EINVAL;
        return -1;
    }
    pipe_t* p = &pipes[idx];
    if (!p->read_open) { errno = EPIPE; return -1; }

    const uint8_t* src = (const uint8_t*)buf;
    size_t written = 0;
    while (written < count && p->count < PIPE_BUF_SIZE) {
        p->buf[p->write_pos] = src[written++];
        p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
        p->count++;
    }
    return (ssize_t)written;
}

/* ======== DIRECTORY ======== */

static DIR_t dir_table[4];

DIR_t* opendir(const char* path) {
    int node;
    if (k_strcmp(path, "/") == 0 || k_strcmp(path, "") == 0) {
        node = -1;
    } else {
        node = vfs_find_path(path);
        if (node < 0) { errno = ENOENT; return NULL; }
        vfs_node_t* n = vfs_get_node(node);
        if (!n || n->type != VFS_DIRECTORY) { errno = ENOTDIR; return NULL; }
    }

    for (int i = 0; i < 4; i++) {
        if (!dir_table[i].active) {
            dir_table[i].parent = node;
            dir_table[i].pos = 0;
            dir_table[i].active = 1;
            return &dir_table[i];
        }
    }
    errno = EMFILE;
    return NULL;
}

static dirent_t dir_entry;

dirent_t* readdir(DIR_t* dir) {
    if (!dir || !dir->active) return NULL;

    int child = vfs_get_child(dir->parent, dir->pos);
    if (child < 0) return NULL;

    vfs_node_t* n = vfs_get_node(child);
    if (!n) return NULL;

    strcpy(dir_entry.name, n->name);
    dir_entry.type = n->type;
    dir_entry.size = (int)n->size;
    dir_entry.node_id = child;
    dir->pos++;

    return &dir_entry;
}

int closedir(DIR_t* dir) {
    if (!dir) return -1;
    dir->active = 0;
    return 0;
}

/* ======== TIME ======== */

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    return inb(0x71);
}

static uint8_t bcd2b(uint8_t v) {
    return ((v >> 4) * 10) + (v & 0x0F);
}

static tm_t current_tm;

time_t time_now(time_t* t) {
    uint8_t s = bcd2b(cmos_read(0x00));
    uint8_t m = bcd2b(cmos_read(0x02));
    uint8_t h = bcd2b(cmos_read(0x04));
    time_t val = (time_t)(h * 3600 + m * 60 + s);
    if (t) *t = val;
    return val;
}

tm_t* localtime(const time_t* t) {
    time_t secs = t ? *t : time_now(0);
    current_tm.tm_hour = (int)(secs / 3600);
    current_tm.tm_min = (int)((secs % 3600) / 60);
    current_tm.tm_sec = (int)(secs % 60);
    current_tm.tm_mday = (int)bcd2b(cmos_read(0x07));
    current_tm.tm_mon = (int)bcd2b(cmos_read(0x08)) - 1;
    current_tm.tm_year = (int)bcd2b(cmos_read(0x09)) + 100; /* 2000 + yil - 1900 */
    current_tm.tm_wday = 0;
    current_tm.tm_yday = 0;
    return &current_tm;
}

int gettimeofday_sec(void) {
    return (int)time_now(0);
}

/* ======== MISC ======== */

static char cwd[128] = "/home";

int isatty(int fd) {
    (void)fd;
    return 1; /* Terminal */
}

char* getcwd(char* buf, size_t size) {
    if (!buf || size == 0) { errno = EINVAL; return NULL; }
    strncpy(buf, cwd, size - 1);
    buf[size - 1] = '\0';
    return buf;
}

int chdir(const char* path) {
    if (!path) { errno = EINVAL; return -1; }

    if (k_strcmp(path, "/") == 0) {
        strcpy(cwd, "/");
        setenv("PWD", cwd, 1);
        return 0;
    }

    int node = vfs_find_path(path);
    if (node < 0) { errno = ENOENT; return -1; }

    vfs_node_t* n = vfs_get_node(node);
    if (!n || n->type != VFS_DIRECTORY) { errno = ENOTDIR; return -1; }

    vfs_build_path(node, cwd);
    setenv("PWD", cwd, 1);
    return 0;
}

int access(const char* path, int mode) {
    (void)mode;
    int node = vfs_find_path(path);
    if (node < 0) { errno = ENOENT; return -1; }
    return 0;
}

unsigned int sleep_sec(unsigned int seconds) {
    msleep(seconds * 1000);
    return 0;
}

int usleep(uint32_t usec) {
    msleep(usec / 1000);
    return 0;
}

long sysconf(int name) {
    switch (name) {
        case _SC_PAGESIZE:    return 4096;
        case _SC_NPROCESSORS: return 1;
        case _SC_PHYS_PAGES:  return (long)(ram_get_total_kb() / 4);
        default: errno = EINVAL; return -1;
    }
}

void abort(void) {
    raise(SIGABRT);
    _exit(134);
    while (1);
}

/* ======== INIT ======== */

void posix_init(void) {
    posix_errno = 0;
    current_pid = 1;
    parent_pid = 0;

    k_memset(sig_handlers, 0, sizeof(sig_handlers));
    k_memset(pipes, 0, sizeof(pipes));
    k_memset(dir_table, 0, sizeof(dir_table));

    env_init();
    strcpy(cwd, "/home");
}
