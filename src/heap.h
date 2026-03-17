#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

/* Heap boyutu: 16 MB */
#define HEAP_SIZE (16 * 1024 * 1024)

/* Blok header */
typedef struct block_header {
    uint32_t size;            /* Veri alani boyutu (header haric) */
    uint32_t magic;           /* 0xDEADBEEF = kullaniliyor, 0xFEEDFACE = bos */
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

#define HEAP_MAGIC_USED 0xDEADBEEF
#define HEAP_MAGIC_FREE 0xFEEDFACE
#define BLOCK_HDR_SIZE  sizeof(block_header_t)
#define MIN_BLOCK_SIZE  16

/* Fonksiyonlar */
void  heap_init(void);
void* kmalloc(uint32_t size);
void  kfree(void* ptr);
void* krealloc(void* ptr, uint32_t new_size);
void* kcalloc(uint32_t count, uint32_t size);

/* Bilgi */
uint32_t heap_get_total(void);
uint32_t heap_get_used(void);
uint32_t heap_get_free(void);
uint32_t heap_get_block_count(void);

#endif
