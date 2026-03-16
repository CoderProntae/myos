#ifndef VESA_H
#define VESA_H

#include <stdint.h>

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
} __attribute__((packed)) multiboot_info_t;

#define COLOR_BG            0x1E1E2E
#define COLOR_TASKBAR       0x2D2D44
#define COLOR_TASKBAR_HOVER 0x3D3D5C
#define COLOR_START_BTN     0x0078D4
#define COLOR_START_HOVER   0x1A8AD4
#define COLOR_WINDOW_BG     0x2B2B3D
#define COLOR_WINDOW_TITLE  0x1F1F2F
#define COLOR_WINDOW_BORDER 0x444466
#define COLOR_TEXT_WHITE     0xFFFFFF
#define COLOR_TEXT_GREY      0xAAAAAA
#define COLOR_TEXT_BLUE      0x5599FF
#define COLOR_TEXT_GREEN     0x55FF55
#define COLOR_TEXT_YELLOW    0xFFFF55
#define COLOR_TEXT_RED       0xFF5555
#define COLOR_TEXT_CYAN      0x55FFFF
#define COLOR_BUTTON         0x3A3A5A
#define COLOR_BUTTON_HOVER   0x4A4A7A
#define COLOR_BUTTON_PRESS   0x0078D4
#define COLOR_PROGRESS_BG    0x333355
#define COLOR_PROGRESS_FG    0x0078D4
#define COLOR_CLOSE_BTN      0xE81123
#define COLOR_CLOSE_HOVER    0xFF2233
#define COLOR_MENU_BG        0x2B2B3D
#define COLOR_MENU_HOVER     0x0078D4
#define COLOR_ACCENT         0x0078D4
#define COLOR_BLACK          0x000000
#define COLOR_TERMINAL_BG    0x0C0C0C

void     vesa_init(multiboot_info_t* mbi);
int      vesa_get_width(void);
int      vesa_get_height(void);
int      vesa_get_bpp(void);
void     vesa_putpixel(int x, int y, uint32_t color);
void     vesa_fill_rect(int x, int y, int w, int h, uint32_t color);
void     vesa_fill_screen(uint32_t color);
void     vesa_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void     vesa_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg);
void     vesa_draw_string_nobg(int x, int y, const char* str, uint32_t fg);
void     vesa_draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void     vesa_draw_rounded_rect(int x, int y, int w, int h, uint32_t color, int r);
void     vesa_copy_buffer(void);

void     vesa_set_depth(int depth);
int      vesa_get_depth(void);
uint32_t vesa_preview_color(uint32_t color, int depth);
void     vesa_putpixel_raw(int x, int y, uint32_t color);

extern uint32_t* backbuffer;

#endif
