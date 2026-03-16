#ifndef ICONS_H
#define ICONS_H

#include "vesa.h"

static inline void draw_icon_computer(int x, int y) {
    vesa_fill_rect(x+2, y+2, 20, 14, 0x333355);
    vesa_fill_rect(x+3, y+3, 18, 12, 0x0078D4);
    vesa_fill_rect(x+4, y+4, 6, 4, 0x3399FF);
    vesa_fill_rect(x+9, y+16, 6, 2, 0x555577);
    vesa_fill_rect(x+7, y+18, 10, 2, 0x555577);
}

static inline void draw_icon_terminal(int x, int y) {
    vesa_fill_rect(x+1, y+2, 22, 18, 0x0C0C0C);
    vesa_draw_rect_outline(x+1, y+2, 22, 18, 0x555577);
    vesa_fill_rect(x+4, y+8, 2, 2, 0x55FF55);
    vesa_fill_rect(x+6, y+10, 2, 2, 0x55FF55);
    vesa_fill_rect(x+4, y+12, 2, 2, 0x55FF55);
    vesa_fill_rect(x+10, y+10, 6, 2, 0xAAAAAA);
}

static inline void draw_icon_info(int x, int y) {
    for (int dy = -10; dy <= 10; dy++)
        for (int dx = -10; dx <= 10; dx++)
            if (dx*dx + dy*dy <= 100)
                vesa_putpixel(x+12+dx, y+12+dy, 0x0078D4);
    vesa_fill_rect(x+11, y+6, 2, 2, COLOR_TEXT_WHITE);
    vesa_fill_rect(x+11, y+10, 2, 8, COLOR_TEXT_WHITE);
}

static inline void draw_icon_power(int x, int y) {
    for (int dy = -9; dy <= 9; dy++)
        for (int dx = -9; dx <= 9; dx++) {
            int d = dx*dx + dy*dy;
            if (d <= 81 && d >= 49)
                vesa_putpixel(x+12+dx, y+12+dy, COLOR_TEXT_RED);
        }
    vesa_fill_rect(x+11, y+3, 2, 10, COLOR_TEXT_RED);
}

static inline void draw_close_button(int x, int y, uint32_t bg) {
    vesa_fill_rect(x, y, 16, 16, bg);
    for (int i = 0; i < 10; i++) {
        vesa_putpixel(x+3+i, y+3+i, COLOR_TEXT_WHITE);
        vesa_putpixel(x+12-i, y+3+i, COLOR_TEXT_WHITE);
        vesa_putpixel(x+4+i, y+3+i, COLOR_TEXT_WHITE);
        vesa_putpixel(x+11-i, y+3+i, COLOR_TEXT_WHITE);
    }
}

static inline void draw_windows_logo(int x, int y, uint32_t color) {
    vesa_fill_rect(x, y, 7, 7, color);
    vesa_fill_rect(x+9, y, 7, 7, color);
    vesa_fill_rect(x, y+9, 7, 7, color);
    vesa_fill_rect(x+9, y+9, 7, 7, color);
}

/* Ayarlar (gear) ikonu - 16x16 */
static inline void draw_icon_gear(int x, int y, uint32_t color) {
    /* Dis cember */
    for (int dy = -7; dy <= 7; dy++)
        for (int dx = -7; dx <= 7; dx++) {
            int d = dx*dx + dy*dy;
            if (d <= 49 && d >= 16)
                vesa_putpixel(x+8+dx, y+8+dy, color);
        }
    /* 4 dis */
    vesa_fill_rect(x+7, y,    2, 3, color);
    vesa_fill_rect(x+7, y+13, 2, 3, color);
    vesa_fill_rect(x,   y+7,  3, 2, color);
    vesa_fill_rect(x+13,y+7,  3, 2, color);
    /* Capraz disler */
    vesa_fill_rect(x+2,  y+2,  2, 2, color);
    vesa_fill_rect(x+12, y+2,  2, 2, color);
    vesa_fill_rect(x+2,  y+12, 2, 2, color);
    vesa_fill_rect(x+12, y+12, 2, 2, color);
    /* Ic bosluk (arka plan rengi ile) */
    for (int dy2 = -3; dy2 <= 3; dy2++)
        for (int dx2 = -3; dx2 <= 3; dx2++)
            if (dx2*dx2 + dy2*dy2 <= 9)
                vesa_putpixel(x+8+dx2, y+8+dy2, COLOR_TASKBAR);
}

/* Ekran ikonu - 24x24 */
static inline void draw_icon_display(int x, int y) {
    /* Monitor cerceve */
    vesa_fill_rect(x+1, y+1, 22, 16, 0x444466);
    /* Ekran */
    vesa_fill_rect(x+2, y+2, 20, 14, 0x1E1E2E);
    /* Renk cubugu */
    vesa_fill_rect(x+4, y+5, 4, 8, 0xFF4444);
    vesa_fill_rect(x+8, y+5, 4, 8, 0x44FF44);
    vesa_fill_rect(x+12, y+5, 4, 8, 0x4444FF);
    vesa_fill_rect(x+16, y+5, 4, 8, 0xFFFF44);
    /* Ayak */
    vesa_fill_rect(x+9, y+17, 6, 2, 0x555577);
    vesa_fill_rect(x+7, y+19, 10, 2, 0x555577);
}

#endif
