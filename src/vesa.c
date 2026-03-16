#include "vesa.h"
#include "font.h"
#include "string.h"

static uint32_t* framebuffer;
static uint32_t  screen_width;
static uint32_t  screen_height;
static uint32_t  screen_pitch;
static uint32_t  screen_bpp;

static uint32_t  backbuf_data[800 * 600];
uint32_t* backbuffer = backbuf_data;

/* Aktif renk derinligi (varsayilan 32) */
static int current_depth = 32;

void vesa_init(multiboot_info_t* mbi) {
    framebuffer   = (uint32_t*)(uint32_t)mbi->framebuffer_addr;
    screen_width  = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch  = mbi->framebuffer_pitch;
    screen_bpp    = mbi->framebuffer_bpp;
    if (screen_width == 0)  screen_width  = 800;
    if (screen_height == 0) screen_height = 600;
    if (screen_bpp == 0)    screen_bpp    = 32;
}

int vesa_get_width(void)  { return (int)screen_width; }
int vesa_get_height(void) { return (int)screen_height; }
int vesa_get_bpp(void)    { return (int)screen_bpp; }

void vesa_set_depth(int depth) { current_depth = depth; }
int  vesa_get_depth(void)      { return current_depth; }

/* ====== Renk derinligi simulasyonu ====== */
static uint32_t apply_depth(uint32_t color) {
    if (current_depth >= 24) return color;

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8)  & 0xFF;
    uint8_t b =  color        & 0xFF;

    switch (current_depth) {
        case 1: {
            /* 2 renk: Siyah veya Beyaz */
            uint8_t lum = (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
            return lum > 127 ? 0xFFFFFF : 0x000000;
        }
        case 4: {
            /* ~16 renk: her kanal 4 seviye */
            static const uint8_t lv[4] = {0, 85, 170, 255};
            r = lv[r >> 6];
            g = lv[g >> 6];
            b = lv[b >> 6];
            break;
        }
        case 8: {
            /* 256 renk: 3-3-2 bit RGB */
            r = (r >> 5) << 5;
            g = (g >> 5) << 5;
            b = (b >> 6) << 6;
            break;
        }
        case 16: {
            /* 65536 renk: 5-6-5 bit RGB */
            r = (r >> 3) << 3;
            g = (g >> 2) << 2;
            b = (b >> 3) << 3;
            break;
        }
    }
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void vesa_putpixel(int x, int y, uint32_t color) {
    if (x >= 0 && x < (int)screen_width && y >= 0 && y < (int)screen_height)
        backbuffer[y * screen_width + x] = apply_depth(color);
}

void vesa_fill_rect(int x, int y, int w, int h, uint32_t color) {
    uint32_t c = apply_depth(color);
    for (int j = y; j < y + h && j < (int)screen_height; j++) {
        if (j < 0) continue;
        for (int i = x; i < x + w && i < (int)screen_width; i++) {
            if (i < 0) continue;
            backbuffer[j * screen_width + i] = c;
        }
    }
}

void vesa_fill_screen(uint32_t color) {
    uint32_t c = apply_depth(color);
    for (uint32_t i = 0; i < screen_width * screen_height; i++)
        backbuffer[i] = c;
}

void vesa_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (c < FONT_FIRST || c > FONT_LAST) c = ' ';
    int idx = c - FONT_FIRST;
    const uint8_t* glyph = font_data[idx];
    uint32_t fg_d = apply_depth(fg);
    uint32_t bg_d = apply_depth(bg);

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        int py = y + row;
        if (py < 0 || py >= (int)screen_height) continue;
        for (int col = 0; col < FONT_WIDTH; col++) {
            int px = x + col;
            if (px < 0 || px >= (int)screen_width) continue;
            backbuffer[py * screen_width + px] = (bits & (0x80 >> col)) ? fg_d : bg_d;
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
        uint32_t fg_d = apply_depth(fg);
        for (int row = 0; row < FONT_HEIGHT; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_WIDTH; col++) {
                if (bits & (0x80 >> col))
                    vesa_putpixel(x + col, y + row, fg_d);
            }
        }
        x += FONT_WIDTH;
        str++;
    }
}

void vesa_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    uint32_t c = apply_depth(color);
    for (int i = x; i < x + w; i++) {
        if (i >= 0 && i < (int)screen_width) {
            if (y >= 0 && y < (int)screen_height)
                backbuffer[y * screen_width + i] = c;
            if (y+h-1 >= 0 && y+h-1 < (int)screen_height)
                backbuffer[(y+h-1) * screen_width + i] = c;
        }
    }
    for (int j = y; j < y + h; j++) {
        if (j >= 0 && j < (int)screen_height) {
            if (x >= 0 && x < (int)screen_width)
                backbuffer[j * screen_width + x] = c;
            if (x+w-1 >= 0 && x+w-1 < (int)screen_width)
                backbuffer[j * screen_width + x + w - 1] = c;
        }
    }
}

void vesa_draw_rounded_rect(int x, int y, int w, int h, uint32_t color, int r) {
    vesa_fill_rect(x + r, y, w - 2*r, h, color);
    vesa_fill_rect(x, y + r, r, h - 2*r, color);
    vesa_fill_rect(x + w - r, y + r, r, h - 2*r, color);
    for (int cy2 = 0; cy2 < r; cy2++) {
        for (int cx = 0; cx < r; cx++) {
            if (cx*cx + cy2*cy2 <= r*r) {
                vesa_putpixel(x+r-cx,     y+r-cy2,     color);
                vesa_putpixel(x+w-r+cx-1, y+r-cy2,     color);
                vesa_putpixel(x+r-cx,     y+h-r+cy2-1, color);
                vesa_putpixel(x+w-r+cx-1, y+h-r+cy2-1, color);
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
