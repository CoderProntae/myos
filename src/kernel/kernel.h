#ifndef KERNEL_H
#define KERNEL_H

/*
 * ============================================
 *            MyOS Kernel Header
 * ============================================
 * Ana kernel tanımlamaları ve fonksiyonları
 * ============================================
 */

#include "../include/types.h"
#include "../include/stdint.h"

// ==================== KERNEL BİLGİSİ ====================

#define KERNEL_NAME             "MyOS"
#define KERNEL_VERSION          "0.3.0"
#define KERNEL_CODENAME         "Alpha"

// ==================== MULTIBOOT ====================

#define MULTIBOOT_MAGIC         0x2BADB002

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
} PACKED multiboot_info_t;

// ==================== VBE MODE INFO ====================

typedef struct {
    uint16_t attributes;
    uint8_t  window_a;
    uint8_t  window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t  w_char;
    uint8_t  y_char;
    uint8_t  planes;
    uint8_t  bpp;
    uint8_t  banks;
    uint8_t  memory_model;
    uint8_t  bank_size;
    uint8_t  image_pages;
    uint8_t  reserved0;
    uint8_t  red_mask;
    uint8_t  red_position;
    uint8_t  green_mask;
    uint8_t  green_position;
    uint8_t  blue_mask;
    uint8_t  blue_position;
    uint8_t  reserved_mask;
    uint8_t  reserved_position;
    uint8_t  direct_color_attributes;
    uint32_t framebuffer;
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
    uint8_t  reserved1[206];
} PACKED vbe_mode_info_t;

// ==================== RENKLER ====================

#define COLOR_BLACK             0x00
#define COLOR_BLUE              0x01
#define COLOR_GREEN             0x02
#define COLOR_CYAN              0x03
#define COLOR_RED               0x04
#define COLOR_MAGENTA           0x05
#define COLOR_BROWN             0x06
#define COLOR_LIGHT_GRAY        0x07
#define COLOR_DARK_GRAY         0x08
#define COLOR_LIGHT_BLUE        0x09
#define COLOR_LIGHT_GREEN       0x0A
#define COLOR_LIGHT_CYAN        0x0B
#define COLOR_LIGHT_RED         0x0C
#define COLOR_LIGHT_MAGENTA     0x0D
#define COLOR_YELLOW            0x0E
#define COLOR_WHITE             0x0F

// ==================== EKRAN ====================

#define VGA_WIDTH               80
#define VGA_HEIGHT              25
#define VGA_MEMORY              0xB8000

// ==================== KERNEL FONKSİYONLARI ====================

// Başlatma
void kernel_main(multiboot_info_t* mboot, uint32_t magic);
void kernel_init(multiboot_info_t* mboot);
void kernel_panic(const char* message);

// Yazdırma
void print(const char* str, uint8_t color);
void print_char(char c, uint8_t color);
void print_int(int32_t num, uint8_t color);
void print_hex(uint32_t num, uint8_t color);
void printf(const char* format, ...);

// Ekran
void clear_screen(void);
void scroll_screen(void);
void set_cursor(int x, int y);
void enable_cursor(uint8_t start, uint8_t end);
void disable_cursor(void);

// ==================== GLOBAL DEĞİŞKENLER ====================

extern uint32_t cursor_x;
extern uint32_t cursor_y;
extern uint32_t screen_width;
extern uint32_t screen_height;
extern uint32_t* framebuffer;
extern uint32_t fb_pitch;
extern uint8_t  fb_bpp;

// ==================== I/O FONKSİYONLARI ====================

// io.h'dan da kullanılabilir ama burada da tanımlayalım
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline void outl(uint16_t port, uint32_t val) {
    asm volatile ("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    asm volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// ==================== MEMORY ====================

static inline void io_wait(void) {
    outb(0x80, 0);
}

// ==================== INTERRUPTS ====================

static inline void cli(void) {
    asm volatile ("cli");
}

static inline void sti(void) {
    asm volatile ("sti");
}

static inline void hlt(void) {
    asm volatile ("hlt");
}

#endif /* KERNEL_H */
