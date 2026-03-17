#include "heap.h"
#include "string.h"

/*
 * Basit linked-list heap allocator
 * 
 * Bellek duzeni:
 *   [header][data][header][data][header][data]...
 * 
 * Her blok:
 *   - header: size, magic, next, prev
 *   - data: kullanicinin gördüğü alan
 * 
 * Free bloklari birlestirme (coalescing) destekler
 */

/* Heap alani — kernel BSS'te 16MB */
static uint8_t heap_area[HEAP_SIZE] __attribute__((aligned(16)));

/* Ilk blok pointer */
static block_header_t* first_block = 0;
static int heap_initialized = 0;

/* Istatistikler */
static uint32_t total_allocated = 0;
static uint32_t block_count = 0;

/* ======== Init ======== */

void heap_init(void) {
    /* Tum heap'i tek bir bos blok olarak ayarla */
    first_block = (block_header_t*)heap_area;
    first_block->size  = HEAP_SIZE - BLOCK_HDR_SIZE;
    first_block->magic = HEAP_MAGIC_FREE;
    first_block->next  = 0;
    first_block->prev  = 0;

    total_allocated = 0;
    block_count = 1;
    heap_initialized = 1;
}

/* ======== Blok Bolme ======== */

static void split_block(block_header_t* block, uint32_t size) {
    /* Kalan alan yeni bir bos blok icin yeterli mi? */
    if (block->size < size + BLOCK_HDR_SIZE + MIN_BLOCK_SIZE)
        return;  /* Bolmeye degmez */

    /* Yeni bos blok olustur */
    uint8_t* new_addr = (uint8_t*)block + BLOCK_HDR_SIZE + size;
    block_header_t* new_block = (block_header_t*)new_addr;

    new_block->size  = block->size - size - BLOCK_HDR_SIZE;
    new_block->magic = HEAP_MAGIC_FREE;
    new_block->next  = block->next;
    new_block->prev  = block;

    if (block->next)
        block->next->prev = new_block;

    block->next = new_block;
    block->size = size;

    block_count++;
}

/* ======== Bos Bloklari Birlestir ======== */

static void merge_forward(block_header_t* block) {
    while (block->next &&
           block->next->magic == HEAP_MAGIC_FREE) {
        block_header_t* next = block->next;

        /* Bitisik mi kontrol et */
        uint8_t* expected = (uint8_t*)block + BLOCK_HDR_SIZE + block->size;
        if ((uint8_t*)next != expected)
            break;

        /* Birlestir */
        block->size += BLOCK_HDR_SIZE + next->size;
        block->next = next->next;
        if (next->next)
            next->next->prev = block;

        /* Eski header'i temizle */
        next->magic = 0;
        block_count--;
    }
}

static void merge_backward(block_header_t* block) {
    while (block->prev &&
           block->prev->magic == HEAP_MAGIC_FREE) {
        block_header_t* prev = block->prev;

        /* Bitisik mi kontrol et */
        uint8_t* expected = (uint8_t*)prev + BLOCK_HDR_SIZE + prev->size;
        if ((uint8_t*)block != expected)
            break;

        /* Birlestir */
        prev->size += BLOCK_HDR_SIZE + block->size;
        prev->next = block->next;
        if (block->next)
            block->next->prev = prev;

        block->magic = 0;
        block = prev;
        block_count--;
    }
}

/* ======== malloc ======== */

void* kmalloc(uint32_t size) {
    if (!heap_initialized) return 0;
    if (size == 0) return 0;

    /* 16-byte alignment */
    size = (size + 15) & ~15;

    /* First-fit arama */
    block_header_t* current = first_block;

    while (current) {
        if (current->magic == HEAP_MAGIC_FREE && current->size >= size) {
            /* Uygun blok bulundu */
            split_block(current, size);
            current->magic = HEAP_MAGIC_USED;
            total_allocated += current->size;

            /* Veri alaninin adresini dondur (header'dan sonra) */
            return (void*)((uint8_t*)current + BLOCK_HDR_SIZE);
        }
        current = current->next;
    }

    /* Bellek yok */
    return 0;
}

/* ======== free ======== */

void kfree(void* ptr) {
    if (!ptr) return;
    if (!heap_initialized) return;

    /* Header'a geri don */
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HDR_SIZE);

    /* Gecerlilik kontrolu */
    if (block->magic != HEAP_MAGIC_USED) return;

    /* Serbest birak */
    total_allocated -= block->size;
    block->magic = HEAP_MAGIC_FREE;

    /* Komsu bos bloklarla birlestir */
    merge_forward(block);
    merge_backward(block);
}

/* ======== realloc ======== */

void* krealloc(void* ptr, uint32_t new_size) {
    if (!ptr) return kmalloc(new_size);
    if (new_size == 0) { kfree(ptr); return 0; }

    block_header_t* block = (block_header_t*)((uint8_t*)ptr - BLOCK_HDR_SIZE);
    if (block->magic != HEAP_MAGIC_USED) return 0;

    /* Mevcut blok yeterli mi? */
    if (block->size >= new_size) return ptr;

    /* Yeni blok ayir, kopyala, eskiyi serbest birak */
    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return 0;

    uint32_t copy_size = block->size < new_size ? block->size : new_size;
    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (uint32_t i = 0; i < copy_size; i++)
        dst[i] = src[i];

    kfree(ptr);
    return new_ptr;
}

/* ======== calloc ======== */

void* kcalloc(uint32_t count, uint32_t size) {
    uint32_t total = count * size;
    void* ptr = kmalloc(total);
    if (ptr) {
        k_memset(ptr, 0, total);
    }
    return ptr;
}

/* ======== Bilgi ======== */

uint32_t heap_get_total(void) {
    return HEAP_SIZE;
}

uint32_t heap_get_used(void) {
    return total_allocated;
}

uint32_t heap_get_free(void) {
    return HEAP_SIZE - total_allocated - (block_count * BLOCK_HDR_SIZE);
}

uint32_t heap_get_block_count(void) {
    return block_count;
}
