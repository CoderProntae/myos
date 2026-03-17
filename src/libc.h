#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stdarg.h>

typedef uint32_t size_t;
typedef int32_t  ssize_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define EOF (-1)
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define O_RDONLY  0x00
#define O_WRONLY  0x01
#define O_RDWR    0x02
#define O_CREAT   0x40
#define O_TRUNC   0x200
#define O_APPEND  0x400

#define FOPEN_MAX 16

/* FILE yapisi */
typedef struct {
    int   fd;
    int   flags;
    int   pos;
    int   size;
    int   eof;
    int   error;
    int   active;
    char  name[64];
} FILE_t;

/* ======== STRING ======== */
size_t  strlen(const char* s);
int     strcmp(const char* a, const char* b);
int     strncmp(const char* a, const char* b, size_t n);
char*   strcpy(char* dst, const char* src);
char*   strncpy(char* dst, const char* src, size_t n);
char*   strcat(char* dst, const char* src);
char*   strncat(char* dst, const char* src, size_t n);
char*   strchr(const char* s, int c);
char*   strrchr(const char* s, int c);
char*   strstr(const char* haystack, const char* needle);
char*   strtok(char* str, const char* delim);
char*   strdup(const char* s);
int     tolower(int c);
int     toupper(int c);
int     isalpha(int c);
int     isdigit(int c);
int     isalnum(int c);
int     isspace(int c);
int     isupper(int c);
int     islower(int c);
int     isprint(int c);

/* ======== MEMORY ======== */
void*   memcpy(void* dst, const void* src, size_t n);
void*   memset(void* dst, int value, size_t n);
void*   memmove(void* dst, const void* src, size_t n);
int     memcmp(const void* a, const void* b, size_t n);
void*   memchr(const void* s, int c, size_t n);

/* ======== HEAP ======== */
void*   malloc(size_t size);
void    free(void* ptr);
void*   calloc(size_t count, size_t size);
void*   realloc(void* ptr, size_t new_size);

/* ======== CONVERSION ======== */
int     atoi(const char* s);
long    atol(const char* s);
char*   itoa_buf(int value, char* str, int base);
long    strtol(const char* s, char** endptr, int base);
double  atof(const char* s);

/* ======== FORMAT ======== */
int     snprintf(char* out, size_t cap, const char* fmt, ...);
int     vsnprintf(char* out, size_t cap, const char* fmt, va_list ap);
int     sprintf(char* out, const char* fmt, ...);
int     sscanf(const char* str, const char* fmt, ...);

/* ======== FILE I/O ======== */
FILE_t* fopen(const char* path, const char* mode);
int     fclose(FILE_t* fp);
size_t  fread(void* buffer, size_t elem_size, size_t count, FILE_t* fp);
size_t  fwrite(const void* buffer, size_t elem_size, size_t count, FILE_t* fp);
int     fseek(FILE_t* fp, long offset, int whence);
long    ftell(FILE_t* fp);
int     feof(FILE_t* fp);
int     ferror(FILE_t* fp);
int     fgetc(FILE_t* fp);
int     fputc(int c, FILE_t* fp);
char*   fgets(char* buf, int size, FILE_t* fp);
int     fputs(const char* s, FILE_t* fp);
int     fprintf(FILE_t* fp, const char* fmt, ...);
int     fflush(FILE_t* fp);

/* ======== LOW LEVEL I/O ======== */
int     open(const char* path, int flags);
ssize_t read(int fd, void* buffer, size_t count);
ssize_t write_fd(int fd, const void* buffer, size_t count);
int     close(int fd);
int     unlink(const char* path);
int     mkdir_path(const char* path);

/* ======== NETWORK ======== */
int     dns_lookup(const char* host, uint8_t ip_out[4]);
int     tcp_open(const uint8_t ip[4], uint16_t port);
int     tcp_write_sock(int sock, const void* data, uint16_t len);
int     tcp_read_sock(int sock, void* buffer, uint16_t max_len);
int     tcp_close_sock(int sock);

/* ======== MISC ======== */
int     abs_val(int x);
int     min_val(int a, int b);
int     max_val(int a, int b);
void    swap_int(int* a, int* b);
void    sort_int(int* arr, int n);
uint32_t rand_next(void);
void    srand_seed(uint32_t s);
uint32_t get_ticks(void);
void    msleep(uint32_t ms);

#endif
