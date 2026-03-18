#include "libc.h"
#include "string.h"
#include "heap.h"
#include "vfs.h"
#include "dns.h"
#include "tcp.h"
#include "task.h"
#include "io.h"

/* ================================================================ */
/*                         STRING                                   */
/* ================================================================ */

size_t strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *(unsigned char*)a - *(unsigned char*)b;
}

int strncmp(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == '\0') return 0;
    }
    return 0;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char* strcat(char* dst, const char* src) {
    char* d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

char* strncat(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (*d) d++;
    size_t i = 0;
    while (i < n && src[i]) { *d++ = src[i++]; }
    *d = '\0';
    return dst;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == 0) ? (char*)s : NULL;
}

char* strrchr(const char* s, int c) {
    const char* last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == 0) return (char*)s;
    return (char*)last;
}

char* strstr(const char* haystack, const char* needle) {
    if (!*needle) return (char*)haystack;
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++) {
        if (strncmp(haystack, needle, nlen) == 0)
            return (char*)haystack;
    }
    return NULL;
}

static char* strtok_state = NULL;

char* strtok(char* str, const char* delim) {
    if (str) strtok_state = str;
    if (!strtok_state) return NULL;

    /* Baslangictaki delimiter'lari atla */
    while (*strtok_state) {
        int is_delim = 0;
        for (const char* d = delim; *d; d++) {
            if (*strtok_state == *d) { is_delim = 1; break; }
        }
        if (!is_delim) break;
        strtok_state++;
    }

    if (!*strtok_state) { strtok_state = NULL; return NULL; }

    char* token = strtok_state;

    /* Sonraki delimiter'i bul */
    while (*strtok_state) {
        for (const char* d = delim; *d; d++) {
            if (*strtok_state == *d) {
                *strtok_state = '\0';
                strtok_state++;
                return token;
            }
        }
        strtok_state++;
    }

    strtok_state = NULL;
    return token;
}

char* strdup(const char* s) {
    size_t len = strlen(s) + 1;
    char* dup = (char*)malloc(len);
    if (dup) memcpy(dup, s, len);
    return dup;
}

int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isprint(int c) { return c >= 32 && c < 127; }

/* ================================================================ */
/*                         MEMORY                                   */
/* ================================================================ */

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void* memset(void* dst, int value, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)value;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i-1] = s[i-1];
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = (const uint8_t*)a;
    const uint8_t* y = (const uint8_t*)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

void* memchr(const void* s, int c, size_t n) {
    const uint8_t* p = (const uint8_t*)s;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == (uint8_t)c) return (void*)(p + i);
    }
    return NULL;
}

/* ================================================================ */
/*                         HEAP                                     */
/* ================================================================ */

void* malloc(size_t size) { return kmalloc((uint32_t)size); }
void  free(void* ptr) { kfree(ptr); }
void* calloc(size_t count, size_t size) { return kcalloc((uint32_t)count, (uint32_t)size); }
void* realloc(void* ptr, size_t new_size) { return krealloc(ptr, (uint32_t)new_size); }

/* ================================================================ */
/*                       CONVERSION                                 */
/* ================================================================ */

int atoi(const char* s) {
    int sign = 1, val = 0;
    while (isspace(*s)) s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (isdigit(*s)) { val = val * 10 + (*s - '0'); s++; }
    return val * sign;
}

long atol(const char* s) { return (long)atoi(s); }

char* itoa_buf(int value, char* str, int base) {
    k_itoa(value, str, base);
    return str;
}

long strtol(const char* s, char** endptr, int base) {
    long val = 0;
    int sign = 1;

    while (isspace(*s)) s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;

    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') { base = 8; s++; }
        else base = 10;
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }

    while (*s) {
        int digit;
        if (isdigit(*s)) digit = *s - '0';
        else if (*s >= 'a' && *s <= 'f') digit = 10 + (*s - 'a');
        else if (*s >= 'A' && *s <= 'F') digit = 10 + (*s - 'A');
        else break;
        if (digit >= base) break;
        val = val * base + digit;
        s++;
    }

    if (endptr) *endptr = (char*)s;
    return val * sign;
}

/* ================================================================ */
/*                        FORMAT                                    */
/* ================================================================ */

static void fmt_char(char* out, size_t cap, size_t* pos, char c) {
    if (*pos + 1 < cap) out[*pos] = c;
    (*pos)++;
}

static void fmt_str(char* out, size_t cap, size_t* pos, const char* s) {
    while (*s) fmt_char(out, cap, pos, *s++);
}

static void fmt_int(char* out, size_t cap, size_t* pos, int val, int base, int pad, char pad_char) {
    char tmp[32];
    int neg = 0;

    if (val < 0 && base == 10) { neg = 1; val = -val; }

    int i = 0;
    if (val == 0) { tmp[i++] = '0'; }
    else {
        while (val > 0) {
            int d = val % base;
            tmp[i++] = d < 10 ? '0' + d : 'a' + (d - 10);
            val /= base;
        }
    }
    if (neg) tmp[i++] = '-';

    /* Padding */
    while (i < pad) tmp[i++] = pad_char;

    /* Ters cevir ve yaz */
    for (int j = i - 1; j >= 0; j--)
        fmt_char(out, cap, pos, tmp[j]);
}

static void fmt_uint(char* out, size_t cap, size_t* pos, unsigned int val, int base, int pad, char pad_char) {
    char tmp[32];
    int i = 0;

    if (val == 0) { tmp[i++] = '0'; }
    else {
        while (val > 0) {
            int d = val % base;
            tmp[i++] = d < 10 ? '0' + d : 'a' + (d - 10);
            val /= base;
        }
    }

    while (i < pad) tmp[i++] = pad_char;

    for (int j = i - 1; j >= 0; j--)
        fmt_char(out, cap, pos, tmp[j]);
}

int vsnprintf(char* out, size_t cap, const char* fmt, va_list ap) {
    size_t pos = 0;
    if (cap == 0) return 0;

    while (*fmt) {
        if (*fmt != '%') {
            fmt_char(out, cap, &pos, *fmt++);
            continue;
        }
        fmt++;

        if (*fmt == '%') { fmt_char(out, cap, &pos, '%'); fmt++; continue; }

        /* Flags */
        char pad_char = ' ';
        if (*fmt == '0') { pad_char = '0'; fmt++; }

        /* Width */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Long modifier */
        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; }
        (void)is_long;

        /* Specifier */
        switch (*fmt) {
            case 'd':
            case 'i': {
                int v = va_arg(ap, int);
                fmt_int(out, cap, &pos, v, 10, width, pad_char);
                break;
            }
            case 'u': {
                unsigned int v = va_arg(ap, unsigned int);
                fmt_uint(out, cap, &pos, v, 10, width, pad_char);
                break;
            }
            case 'x': {
                unsigned int v = va_arg(ap, unsigned int);
                fmt_uint(out, cap, &pos, v, 16, width, pad_char);
                break;
            }
            case 'X': {
                unsigned int v = va_arg(ap, unsigned int);
                char tmp2[32];
                int ti = 0;
                if (v == 0) tmp2[ti++] = '0';
                else { while (v > 0) { int d = v % 16; tmp2[ti++] = d < 10 ? '0' + d : 'A' + (d - 10); v /= 16; } }
                while (ti < width) tmp2[ti++] = pad_char;
                for (int j = ti - 1; j >= 0; j--) fmt_char(out, cap, &pos, tmp2[j]);
                break;
            }
            case 'p': {
                unsigned int v = va_arg(ap, unsigned int);
                fmt_str(out, cap, &pos, "0x");
                fmt_uint(out, cap, &pos, v, 16, 8, '0');
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                fmt_str(out, cap, &pos, s);
                break;
            }
            case 'c': {
                char c = (char)va_arg(ap, int);
                fmt_char(out, cap, &pos, c);
                break;
            }
            case 'n': {
                int* np = va_arg(ap, int*);
                if (np) *np = (int)pos;
                break;
            }
            default:
                fmt_char(out, cap, &pos, '%');
                if (*fmt) fmt_char(out, cap, &pos, *fmt);
                break;
        }
        if (*fmt) fmt++;
    }

    if (pos >= cap) pos = cap - 1;
    out[pos] = '\0';
    return (int)pos;
}

int snprintf(char* out, size_t cap, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(out, cap, fmt, ap);
    va_end(ap);
    return ret;
}

int sprintf(char* out, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(out, 4096, fmt, ap);
    va_end(ap);
    return ret;
}

/* Basit sscanf — sadece %d ve %s destekler */
int sscanf(const char* str, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int matched = 0;

    while (*fmt && *str) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                int* p = va_arg(ap, int*);
                int sign = 1, val = 0;
                int found = 0;
                while (isspace(*str)) str++;
                if (*str == '-') { sign = -1; str++; }
                while (isdigit(*str)) { val = val * 10 + (*str - '0'); str++; found = 1; }
                if (found) { *p = val * sign; matched++; }
                else break;
                fmt++;
            } else if (*fmt == 's') {
                char* p = va_arg(ap, char*);
                while (isspace(*str)) str++;
                while (*str && !isspace(*str)) *p++ = *str++;
                *p = '\0';
                matched++;
                fmt++;
            } else {
                break;
            }
        } else if (isspace(*fmt)) {
            while (isspace(*str)) str++;
            fmt++;
        } else {
            if (*fmt != *str) break;
            fmt++;
            str++;
        }
    }

    va_end(ap);
    return matched;
}

/* ================================================================ */
/*                        FILE I/O                                  */
/* ================================================================ */

static FILE_t file_table[FOPEN_MAX];
static int file_table_init = 0;

static void init_file_table(void) {
    if (file_table_init) return;
    memset(file_table, 0, sizeof(file_table));
    file_table_init = 1;
}

FILE_t* fopen(const char* path, const char* mode) {
    init_file_table();

    int flags = 0;
    int create = 0;

    if (strcmp(mode, "r") == 0) flags = O_RDONLY;
    else if (strcmp(mode, "w") == 0) { flags = O_WRONLY | O_CREAT | O_TRUNC; create = 1; }
    else if (strcmp(mode, "a") == 0) { flags = O_WRONLY | O_CREAT | O_APPEND; create = 1; }
    else if (strcmp(mode, "r+") == 0) flags = O_RDWR;
    else if (strcmp(mode, "w+") == 0) { flags = O_RDWR | O_CREAT | O_TRUNC; create = 1; }
    else flags = O_RDONLY;

    int fd = vfs_find_path(path);

    if (fd < 0 && create) {
        /* Dosya yok, olustur */
        fd = vfs_create_file(path, -1, "", 0);
    }
    if (fd < 0) return NULL;

    /* Bos slot bul */
    for (int i = 0; i < FOPEN_MAX; i++) {
        if (!file_table[i].active) {
            FILE_t* fp = &file_table[i];
            fp->fd = fd;
            fp->flags = flags;
            fp->pos = 0;
            fp->eof = 0;
            fp->error = 0;
            fp->active = 1;
            strncpy(fp->name, path, sizeof(fp->name) - 1);

            vfs_node_t* n = vfs_get_node(fd);
            fp->size = n ? (int)n->size : 0;

            if (flags & O_APPEND) fp->pos = fp->size;
            if (flags & O_TRUNC) {
                vfs_write_file(fd, "", 0);
                fp->size = 0;
                fp->pos = 0;
            }

            return fp;
        }
    }
    return NULL;
}

int fclose(FILE_t* fp) {
    if (!fp || !fp->active) return EOF;
    fp->active = 0;
    return 0;
}

size_t fread(void* buffer, size_t elem_size, size_t count, FILE_t* fp) {
    if (!fp || !fp->active) return 0;

    size_t total = elem_size * count;
    vfs_node_t* n = vfs_get_node(fp->fd);
    if (!n || !n->data) { fp->eof = 1; return 0; }

    size_t avail = (fp->pos < (int)n->size) ? n->size - fp->pos : 0;
    if (total > avail) total = avail;
    if (total == 0) { fp->eof = 1; return 0; }

    uint8_t* dst = (uint8_t*)buffer;
    for (size_t i = 0; i < total; i++)
        dst[i] = n->data[fp->pos + i];

    fp->pos += (int)total;
    if ((uint32_t)fp->pos >= n->size) fp->eof = 1;

    return total / elem_size;
}

size_t fwrite(const void* buffer, size_t elem_size, size_t count, FILE_t* fp) {
    if (!fp || !fp->active) return 0;

    size_t total = elem_size * count;
    if (total == 0) return 0;

    if (fp->flags & O_APPEND) {
        vfs_append_file(fp->fd, buffer, (uint32_t)total);
    } else {
        vfs_write_file(fp->fd, buffer, (uint32_t)total);
    }

    vfs_node_t* n = vfs_get_node(fp->fd);
    if (n) fp->size = (int)n->size;
    fp->pos += (int)total;

    return count;
}

int fseek(FILE_t* fp, long offset, int whence) {
    if (!fp || !fp->active) return -1;

    int new_pos;
    switch (whence) {
        case SEEK_SET: new_pos = (int)offset; break;
        case SEEK_CUR: new_pos = fp->pos + (int)offset; break;
        case SEEK_END: new_pos = fp->size + (int)offset; break;
        default: return -1;
    }

    if (new_pos < 0) new_pos = 0;
    fp->pos = new_pos;
    fp->eof = 0;
    return 0;
}

long ftell(FILE_t* fp) {
    if (!fp || !fp->active) return -1;
    return (long)fp->pos;
}

int feof(FILE_t* fp) { return fp ? fp->eof : 1; }
int ferror(FILE_t* fp) { return fp ? fp->error : 1; }

int fgetc(FILE_t* fp) {
    uint8_t c;
    if (fread(&c, 1, 1, fp) == 1) return (int)c;
    return EOF;
}

int fputc(int c, FILE_t* fp) {
    uint8_t ch = (uint8_t)c;
    if (fwrite(&ch, 1, 1, fp) == 1) return c;
    return EOF;
}

char* fgets(char* buf, int size, FILE_t* fp) {
    if (!fp || !fp->active || size <= 0) return NULL;

    int i = 0;
    while (i < size - 1) {
        int c = fgetc(fp);
        if (c == EOF) break;
        buf[i++] = (char)c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

int fputs(const char* s, FILE_t* fp) {
    size_t len = strlen(s);
    return (fwrite(s, 1, len, fp) == len) ? 0 : EOF;
}

int fprintf(FILE_t* fp, const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fwrite(buf, 1, (size_t)len, fp);
    return len;
}

int fflush(FILE_t* fp) {
    (void)fp;
    return 0;
}

/* ================================================================ */
/*                      LOW LEVEL I/O                               */
/* ================================================================ */

int open(const char* path, int flags) {
    int fd = vfs_find_path(path);
    if (fd < 0 && (flags & O_CREAT)) {
        fd = vfs_create_file(path, -1, "", 0);
    }
    return fd;
}

ssize_t read(int fd, void* buffer, size_t count) {
    return (ssize_t)vfs_read_file(fd, buffer, (uint32_t)count);
}

ssize_t write_fd(int fd, const void* buffer, size_t count) {
    return (ssize_t)vfs_write_file(fd, buffer, (uint32_t)count);
}

int close(int fd) { (void)fd; return 0; }

int unlink(const char* path) {
    int node = vfs_find_path(path);
    if (node < 0) return -1;
    return vfs_delete(node);
}

int mkdir_path(const char* path) {
    return vfs_create_dir(path, -1);
}

/* ================================================================ */
/*                        NETWORK                                   */
/* ================================================================ */

int dns_lookup(const char* host, uint8_t ip_out[4]) {
    return dns_resolve(host, ip_out);
}

int tcp_open(const uint8_t ip[4], uint16_t port) {
    return tcp_connect(ip, port);
}

int tcp_write_sock(int sock, const void* data, uint16_t len) {
    return tcp_send(sock, data, len);
}

int tcp_read_sock(int sock, void* buffer, uint16_t max_len) {
    return tcp_recv(sock, buffer, max_len);
}

int tcp_close_sock(int sock) {
    return tcp_close(sock);
}

/* ================================================================ */
/*                          MISC                                    */
/* ================================================================ */

int abs_val(int x) { return x < 0 ? -x : x; }
int min_val(int a, int b) { return a < b ? a : b; }
int max_val(int a, int b) { return a > b ? a : b; }

void swap_int(int* a, int* b) {
    int t = *a; *a = *b; *b = t;
}

/* Basit bubble sort */
void sort_int(int* arr, int n) {
    for (int i = 0; i < n - 1; i++)
        for (int j = 0; j < n - i - 1; j++)
            if (arr[j] > arr[j+1]) swap_int(&arr[j], &arr[j+1]);
}

static uint32_t rand_state = 12345;

void srand_seed(uint32_t s) { rand_state = s; }

uint32_t rand_next(void) {
    rand_state = rand_state * 1103515245 + 12345;
    return (rand_state >> 16) & 0x7FFF;
}

uint32_t get_ticks(void) {
    return task_get_ticks();
}

void msleep(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 500; i++);
}
