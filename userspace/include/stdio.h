#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdint.h>

#define EOF (-1)
#define BUFSIZ 1024

typedef struct _FILE {
    int fd;
    int flags;
    int ungetc_char;
    char* buffer;
    size_t buf_pos;
    size_t buf_size;
    int eof;
    int error;
} FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

/* File operations */
FILE* fopen(const char* pathname, const char* mode);
FILE* freopen(const char* pathname, const char* mode, FILE* stream);
int fclose(FILE* stream);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);
int fflush(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void clearerr(FILE* stream);
int remove(const char* pathname);
int rename(const char* oldpath, const char* newpath);

/* Formatted I/O */
int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int sprintf(char* str, const char* format, ...);
int snprintf(char* str, size_t size, const char* format, ...);
int vprintf(const char* format, void* ap);
int vfprintf(FILE* stream, const char* format, void* ap);
int vsprintf(char* str, const char* format, void* ap);
int vsnprintf(char* str, size_t size, const char* format, void* ap);

/* Character I/O */
int fgetc(FILE* stream);
char* fgets(char* s, int size, FILE* stream);
int fputc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
int getc(FILE* stream);
int getchar(void);
char* gets(char* s);
int putc(int c, FILE* stream);
int putchar(int c);
int puts(const char* s);
int ungetc(int c, FILE* stream);

/* Temporary files */
FILE* tmpfile(void);
char* tmpnam(char* s);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#endif
