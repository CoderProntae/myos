#ifndef _TYPES_H
#define _TYPES_H

/*
 * ============================================
 *           MyOS Type Definitions
 * ============================================
 * Sistem genelinde kullanılan tipler
 * ============================================
 */

#include "stdint.h"

// ==================== TEMEL TİPLER ====================

typedef uint32_t                size_t;
typedef int32_t                 ssize_t;
typedef int32_t                 ptrdiff_t;

typedef int32_t                 pid_t;      // Process ID
typedef int32_t                 tid_t;      // Thread ID
typedef uint32_t                uid_t;      // User ID
typedef uint32_t                gid_t;      // Group ID

typedef int32_t                 off_t;      // File offset
typedef uint32_t                mode_t;     // File mode
typedef uint32_t                dev_t;      // Device ID
typedef uint32_t                ino_t;      // Inode number
typedef uint32_t                nlink_t;    // Link count
typedef int32_t                 blksize_t;  // Block size
typedef int32_t                 blkcnt_t;   // Block count

typedef int64_t                 time_t;     // Time
typedef uint32_t                useconds_t; // Microseconds
typedef int32_t                 suseconds_t;// Signed microseconds
typedef uint32_t                clock_t;    // Clock ticks

// ==================== BOOLEAN ====================

#ifndef __cplusplus
typedef _Bool                   bool;
#define true                    1
#define false                   0
#endif

// ==================== VARIADIC ====================

typedef __builtin_va_list       va_list;
#define va_start(v, l)          __builtin_va_start(v, l)
#define va_end(v)               __builtin_va_end(v)
#define va_arg(v, l)            __builtin_va_arg(v, l)
#define va_copy(d, s)           __builtin_va_copy(d, s)

// ==================== ATTRIBUTES ====================

#define PACKED                  __attribute__((packed))
#define ALIGNED(x)              __attribute__((aligned(x)))
#define NORETURN                __attribute__((noreturn))
#define UNUSED                  __attribute__((unused))
#define WEAK                    __attribute__((weak))
#define INLINE                  static inline __attribute__((always_inline))

// ==================== OFFSETOF ====================

#define offsetof(type, member)  __builtin_offsetof(type, member)

// ==================== MIN/MAX ====================

#define MIN(a, b)               ((a) < (b) ? (a) : (b))
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define CLAMP(x, lo, hi)        MIN(MAX(x, lo), hi)

// ==================== BIT İŞLEMLERİ ====================

#define BIT(n)                  (1U << (n))
#define SET_BIT(x, n)           ((x) |= BIT(n))
#define CLEAR_BIT(x, n)         ((x) &= ~BIT(n))
#define TOGGLE_BIT(x, n)        ((x) ^= BIT(n))
#define CHECK_BIT(x, n)         (((x) >> (n)) & 1)

// ==================== ARRAY ====================

#define ARRAY_SIZE(arr)         (sizeof(arr) / sizeof((arr)[0]))

// ==================== ALIGN ====================

#define ALIGN_UP(x, align)      (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(x, align)    ((x) & ~((align) - 1))
#define IS_ALIGNED(x, align)    (((x) & ((align) - 1)) == 0)

// ==================== MEMORY SIZES ====================

#define KB                      (1024UL)
#define MB                      (1024UL * KB)
#define GB                      (1024UL * MB)

// ==================== PAGE ====================

#define PAGE_SIZE               4096
#define PAGE_MASK               (~(PAGE_SIZE - 1))
#define PAGE_ALIGN(addr)        ALIGN_UP(addr, PAGE_SIZE)

#endif /* _TYPES_H */
