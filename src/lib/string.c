#include "string.h"

int k_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

int k_strcmp(const char* s1, const char* s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int k_strncmp(const char* s1, const char* s2, int n) {
    for (int i = 0; i < n; i++) {
        if (s1[i] != s2[i]) return s1[i] - s2[i];
        if (s1[i] == '\0') return 0;
    }
    return 0;
}

char* k_strcpy(char* dest, const char* src) {
    char* d = dest;
    while ((*d++ = *src++));
    return dest;
}

void* k_memset(void* ptr, int value, uint32_t size) {
    uint8_t* p = (uint8_t*)ptr;
    for (uint32_t i = 0; i < size; i++) p[i] = (uint8_t)value;
    return ptr;
}

void k_itoa(int num, char* str, int base) {
    int i = 0, neg = 0;
    if (num == 0) { str[0] = '0'; str[1] = '\0'; return; }
    if (num < 0 && base == 10) { neg = 1; num = -num; }
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num /= base;
    }
    if (neg) str[i++] = '-';
    str[i] = '\0';
    for (int s = 0, e = i - 1; s < e; s++, e--) {
        char tmp = str[s]; str[s] = str[e]; str[e] = tmp;
    }
}
