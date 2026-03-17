#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned int size_t;
typedef signed int ptrdiff_t;
typedef unsigned int wchar_t;
typedef int intptr_t;
typedef unsigned int uintptr_t;

#ifndef NULL
#define NULL ((void*)0)
#endif

#define offsetof(type, member) ((size_t)&((type *)0)->member)

#endif
