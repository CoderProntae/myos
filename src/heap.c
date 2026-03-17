#include "heap.h"
#include "string.h"

static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(16)));
static block_header_t* first_block = 0;
static int heap_initialized = 0;

void heap_init(void) {
    first_block = (block_header_t*)heap_area;
    first_block->size  = HEAP_SIZE - BLOCK_HDR_SIZE;
    first_block->magic = HEAP_MAGIC_FREE;
    first_block->next  = 0;
    first_block->prev  = 0;
    heap_initialized = 1;
}

/* ======== Split ======== */

static void split_block(block_header_t* block, uint32_t size) {
    uint32_t remaining = block->size - size;
    if (remaining <= BLOCK_HDR_SIZE + MIN_BLOCK_SIZE)
        return;

    block_header_t* new_block = (block_header_t*)((uint8_t*)block + BLOCK_HDR_SIZE + size);
    new_block->size  = remaining - BLOCK_HDR_SIZE;
    new_block->magic = HEAP_MAGIC_FREE;
    new_block->next  = block->next;
    new_block->prev  = block;

    if (block->next)
        block->next->prev = new_block;

    block->next = new_block;
    block->size = size;
}

/* ======== Merge ======== */

static block_header_t* try_merge(block_header_t* block) {
    if (!block) return block;

    /* Ileri: bu blok + sonraki blok */
    if (block->next && block->next->magic == HEAP_MAGIC_FREE) {
        /* Bitisik mi? */
        uint8_t* end_of_block = (uint8_t*)block + BLOCK_HDR_SIZE + block->size;
        if (end_of_block == (uint8_t*)block->next) {
            block_header_t* next = block->next;
            block->size += BLOCK_HDR_SIZE + next->size;
            block->next = next->next;
            if (next->next)
                next->next->prev = block;
            next->magic = 0;
        }
    }

    /* Geri: onceki blok + bu blok */
    if (block->prev && block->prev->magic == HEAP_MAGIC_FREE) {
        uint8_t* end_of_prev = (uint8_t*)block->prev + BLOCK_HDR_SIZE + block->prev->size;
        if (end_of_prev == (uint8_t*)block) {
            block_header_t* prev = block->prev;
            prev->size += BLOCK_HDR_SIZE + block->size;
            prev->next = block->next;
            if (block->next)
                block->next->prev = prev;
            block->magic = 0;
            block = prev;
        }
    }

    return block;
}

/* ======== malloc ======== */

void* kmalloc(uint32_t size) {
    if (!heap_initialized || size == 0) return 0;

    /* 16-byte align */
    size = (size + 15) & ~15;

    block_header_t* cur = first_block;
    while (cur) {
        if (cur->magic == HEAP_MAGIC_FREE && cur->size >= size) {
            split_block(cur, size);
            cur->magic = HEAP_MAGIC_USED;
            return (void*)((uint8_t*)cur + BLOCK_HDR_SIZE);
        }
        cur = cur->next;
    }
    return 0;
}

/* ======== free ======== */

void kfree(void* ptr) {
    if (!ptr || !heap_initialized) return;

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HDR_SIZE);

    /* Magic kontrolu */
    if (block->magic != HEAP_MAGIC_USED) return;

    /* Serbest birak */
    block->magic = HEAP_MAGIC_FREE;

    /* Veriyi temizle */
    k_memset((uint8_t*)block + BLOCK_HDR_SIZE, 0, block->size);

    /* Bitisik bos bloklarla birlestir */
    try_merge(block);
}

/* ======== realloc ======== */

void* krealloc(void* ptr, uint32_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return 0; }

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HDR_SIZE);
    if (block->magic != HEAP_MAGIC_USED) return 0;

    new_size = (new_size + 15) & ~15;
    if (block->size >= new_size) return ptr;

    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return 0;

    uint32_t copy_size = block->size < new_size ? block->size : new_size;
    uint8_t* s = (uint8_t*)ptr;
    uint8_t* d = (uint8_t*)new_ptr;
    for (uint32_t i = 0; i < copy_size; i++) d[i] = s[i];

    kfree(ptr);
    return new_ptr;
}

/* ======== calloc ======== */

void* kcalloc(uint32_t count, uint32_t size) {
    uint32_t total = count * size;
    void* ptr = kmalloc(total);
    if (ptr) k_memset(ptr, 0, total);
    return ptr;
}

/* ======== Bilgi — Linked List Tarama ======== */

uint32_t heap_get_total(void) {
    return HEAP_SIZE;
}

uint32_t heap_get_used(void) {
    if (!heap_initialized) return 0;
    uint32_t used = 0;
    block_header_t* cur = first_block;
    while (cur) {
        if (cur->magic == HEAP_MAGIC_USED)
            used += cur->size;
        cur = cur->next;
    }
    return used;
}

uint32_t heap_get_free(void) {
    if (!heap_initialized) return 0;
    uint32_t fb = 0;
    block_header_t* cur = first_block;
    while (cur) {
        if (cur->magic == HEAP_MAGIC_FREE)
            fb += cur->size;
        cur = cur->next;
    }
    return fb;
}

uint32_t heap_get_block_count(void) {
    if (!heap_initialized) return 0;
    uint32_t count = 0;
    block_header_t* cur = first_block;
    while (cur) {
        count++;
        cur = cur->next;
    }
    return count;
}

/* ======== RAM ======== */

static uint32_t real_ram_lower_kb = 0;
static uint32_t real_ram_upper_kb = 0;
static uint32_t real_ram_total_kb = 0;

void ram_detect(uint32_t mem_lower, uint32_t mem_upper) {
    real_ram_lower_kb = mem_lower;
    real_ram_upper_kb = mem_upper;
    real_ram_total_kb = mem_upper + 1024;
}

uint32_t ram_get_total_kb(void) { return real_ram_total_kb; }
uint32_t ram_get_total_mb(void) { return real_ram_total_kb / 1024; }
uint32_t ram_get_lower_kb(void) { return real_ram_lower_kb; }
uint32_t ram_get_upper_kb(void) { return real_ram_upper_kb; }
