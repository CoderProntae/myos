#ifndef STRING_H
#define STRING_H

#include <stdint.h>

int   k_strlen(const char* str);
int   k_strcmp(const char* s1, const char* s2);
int   k_strncmp(const char* s1, const char* s2, int n);
char* k_strcpy(char* dest, const char* src);
void* k_memset(void* ptr, int value, uint32_t size);
void  k_itoa(int num, char* str, int base);

#endif
