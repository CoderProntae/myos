#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

/* Memory */
void* malloc(size_t size);
void* calloc(size_t nmemb, size_t size);
void* realloc(void* ptr, size_t size);
void free(void* ptr);

/* Program control */
void exit(int status);
void abort(void);
int atexit(void (*func)(void));
void _exit(int status);

/* String conversion */
int atoi(const char* nptr);
long atol(const char* nptr);
double atof(const char* nptr);
long strtol(const char* nptr, char** endptr, int base);
unsigned long strtoul(const char* nptr, char** endptr, int base);
double strtod(const char* nptr, char** endptr);

/* Searching and sorting */
void qsort(void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));
void* bsearch(const void* key, const void* base, size_t nmemb, size_t size, int (*compar)(const void*, const void*));

/* Math */
int abs(int x);
long labs(long x);
int rand(void);
void srand(unsigned int seed);

/* Multibyte/wide */
int mbtowc(wchar_t* pwc, const char* s, size_t n);
int wctomb(char* s, wchar_t wchar);
size_t mbstowcs(wchar_t* dest, const char* src, size_t n);
size_t wcstombs(char* dest, const wchar_t* src, size_t n);

/* Environment */
char* getenv(const char* name);
int system(const char* command);

/* Integer arithmetic */
typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;

div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);

#endif
