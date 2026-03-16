#ifndef IO_H
#define IO_H

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void io_wait(void) { outb(0x80, 0); }

/* Hizli bellek kopyalama (rep movsd) */
static inline void fast_memcpy32(void* dst, const void* src, uint32_t count) {
    __asm__ __volatile__(
        "rep movsl"
        : "+D"(dst), "+S"(src), "+c"(count)
        : : "memory"
    );
}

/* Hizli bellek doldurma (rep stosd) */
static inline void fast_memset32(void* dst, uint32_t val, uint32_t count) {
    __asm__ __volatile__(
        "rep stosl"
        : "+D"(dst), "+c"(count)
        : "a"(val) : "memory"
    );
}

#endif
