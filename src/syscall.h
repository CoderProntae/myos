#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Syscall numaralari */
#define SYS_EXIT        0
#define SYS_PRINT       1
#define SYS_GETCHAR     2
#define SYS_MALLOC      3
#define SYS_FREE        4
#define SYS_OPEN        5
#define SYS_READ        6
#define SYS_WRITE       7
#define SYS_CLOSE       8
#define SYS_TIME        9
#define SYS_SLEEP       10
#define SYS_PUTPIXEL    11
#define SYS_SCREEN_W    12
#define SYS_SCREEN_H    13
#define SYS_NET_SEND    14
#define SYS_NET_RECV    15
#define SYS_DNS         16
#define SYS_TCP_CONNECT 17
#define SYS_TCP_SEND    18
#define SYS_TCP_RECV    19
#define SYS_TCP_CLOSE   20
#define SYS_MEMINFO     21
#define SYS_MKDIR       22
#define SYS_READDIR     23
#define SYS_DELETE      24
#define SYS_STRLEN      25
#define SYS_STRCMP       26
#define SYS_FILL_RECT   27
#define SYS_DRAW_STR    28
#define SYS_FLUSH       29
#define SYS_MAX         30

/* Syscall sonucu */
typedef struct {
    int32_t  retval;
    uint32_t extra;
} syscall_result_t;

/* Fonksiyonlar */
void           syscall_init(void);
syscall_result_t syscall_handle(uint32_t num, uint32_t arg1,
                                 uint32_t arg2, uint32_t arg3,
                                 uint32_t arg4);

/* Program tarafindan cagrilan wrapper */
static inline int32_t sys_call(uint32_t num, uint32_t a1, uint32_t a2,
                                uint32_t a3, uint32_t a4) {
    int32_t ret;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2), "d"(a3), "S"(a4)
        : "memory"
    );
    return ret;
}

#endif
