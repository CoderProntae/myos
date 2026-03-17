#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

#define HEAP_SIZE (16 * 1024 * 1024)

typedef struct block_header {
    uint32_t size;
    uint32_t magic;
    struct block_header* next;
    struct block_header* prev;
} block_header_t;

#define HEAP_MAGIC_USED 0xDEADBEEF
#define HEAP_MAGIC_FREE 0xFEEDFACE
#define BLOCK_HDR_SIZE  sizeof(block_header_t)
#define MIN_BLOCK_SIZE  16

void  heap_init(void);
void* kmalloc(uint32_t size);
void  kfree(void* ptr);
void* krealloc(void* ptr, uint32_t new_size);
void* kcalloc(uint32_t count, uint32_t size);

uint32_t heap_get_total(void);
uint32_t heap_get_used(void);
uint32_t heap_get_free(void);
uint32_t heap_get_block_count(void);

/* Gercek RAM bilgisi */
void     ram_detect(uint32_t mem_lower, uint32_t mem_upper);
uint32_t ram_get_total_kb(void);
uint32_t ram_get_total_mb(void);
uint32_t ram_get_lower_kb(void);
uint32_t ram_get_upper_kb(void);

#endif
