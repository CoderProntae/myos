#include "vesa.h"
#include "font.h"
#include "string.h"

static uint32_t* framebuffer;
static uint32_t  screen_width;
static uint32_t  screen_height;
static uint32_t  screen_pitch;
static uint32_t  screen_bpp;

/* Double buffer - titresim yok */
static uint32_t  backbuf_data[800 * 600];
uint32_t* backbuffer = backbuf_data;

void vesa_init(multiboot_info_t* mbi) {
    framebuffer  = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
    screen_width  = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch  = mbi->framebuffer_pitch;
    screen_bpp    = mbi->framebuffer_bpp;

    if (screen_width == 0) screen_width = 800;
    if (screen_height == 0) screen_height = 600;
}

int vesa_get_width(void)  { return (int)screen_width; }
int vesa_get_height(void) { return (int)screen_height; }

void vesa_putpixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)screen_width && y >= 0 && y < (int)screen_height)
        backbuffer[y * screen_width + x] = color;
}

void vesa_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = y; j < y + h && j < (int)screen_height; j++) {
        if (j < 0) continue;
        for (int i = x; i < x + w && i < (int)screen_width; i++) {
            if (i < 0) continue;
            backbuffer[j * screen_width + i] = color;
        }
    }
}

void vesa_fill_screen(uint32_t color) {
    for (uint32_t i = 0; i < screen_width * screen_height; i++)
        backbuffer[i] = color;
}

void vesa_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (c < FONT_FIRST || c > FONT_LAST) c = ' ';
    int idx = c - FONT_FIRST;
    const uint8_t* glyph = font_data[idx];

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            uint32_t color = (bits & (0x80 >> col)) ? fg : bg;
            vesa_putpixel(x + col, y + row, color);
        }
    }
}

void vesa_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    while (*str) {
        if (*str == '\n') { y += FONT_HEIGHT; x = 0; str++; continue; }
        vesa_draw_char(x, y, *str, fg, bg);
        x += FONT_WIDTH;
        str++;
    }
}

void vesa_draw_string_nobg(int x, int y, const char* str, uint32_t fg) {
    while (*str) {
        if (*str == '\n') { y += FONT_HEIGHT; x = 0; str++; continue; }
        if (*str < FONT_FIRST || *str > FONT_LAST) { x += FONT_WIDTH; str++; continue; }
        int idx = *str - FONT_FIRST;
        const uint8_t* glyph = font_data[idx];
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (bits & (0x80 >> col))
                    vesa_putpixel(x + col, y + row, fg);
            }
        }
        x += FONT_WIDTH;
        str++;
    }
}

void vesa_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int i = x; i < x + w; i++) {
        vesa_putpixel(i, y, color);
        vesa_putpixel(i, y + h - 1, color);
    }
    for (int j = y; j < y + h; j++) {
        vesa_putpixel(x, j, color);
        vesa_putpixel(x + w - 1, j, color);
    }
}

void vesa_draw_rounded_rect(int x, int y, int w, int h, uint32_t color, int r) {
    vesa_fill_rect(x + r, y, w - 2 * r, h, color);
    vesa_fill_rect(x, y + r, r, h - 2 * r, color);
    vesa_fill_rect(x + w - r, y + r, r, h - 2 * r, color);

    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            if (cx * cx + cy * cy <= r * r) {
                vesa_putpixel(x + r - cx, y + r - cy, color);
                vesa_putpixel(x + w - r + cx - 1, y + r - cy, color);
                vesa_putpixel(x + r - cx, y + h - r + cy - 1, color);
                vesa_putpixel(x + w - r + cx - 1, y + h - r + cy - 1, color);
            }
        }
    }
}

void vesa_copy_buffer(void) {
    uint32_t* dst = framebuffer;
    uint32_t* src = backbuffer;
    uint32_t total = screen_width * screen_height;
    for (uint32_t i = 0; i < total; i++)
        dst[i] = src[i];
}
