#ifndef ICONS_H
#define ICONS_H

#include "vesa.h"

/* 24x24 bilgisayar ikonu */
static inline void draw_icon_computer(int x, int y) {
    /* Monitor */
    vesa_fill_rect(x+2, y+2, 20, 14, 0x333355);
    vesa_fill_rect(x+3, y+3, 18, 12, 0x0078D4);
    /* Ekran parlama */
    vesa_fill_rect(x+4, y+4, 6, 4, 0x3399FF);
    /* Stand */
    vesa_fill_rect(x+9, y+16, 6, 2, 0x555577);
    vesa_fill_rect(x+7, y+18, 10, 2, 0x555577);
}

/* 24x24 terminal ikonu */
static inline void draw_icon_terminal(int x, int y) {
    vesa_fill_rect(x+1, y+2, 22, 18, 0x0C0C0C);
    vesa_draw_rect_outline(x+1, y+2, 22, 18, 0x555577);
    /* > prompt */
    vesa_fill_rect(x+4, y+8, 2, 2, 0x55FF55);
    vesa_fill_rect(x+6, y+10, 2, 2, 0x55FF55);
    vesa_fill_rect(x+4, y+12, 2, 2, 0x55FF55);
    /* cursor */
    vesa_fill_rect(x+10, y+10, 6, 2, 0xAAAAAA);
}

/* 24x24 bilgi ikonu */
static inline void draw_icon_info(int x, int y) {
    /* Daire */
    for (int dy = -10; dy <= 10; dy++)
        for (int dx = -10; dx <= 10; dx++)
            if (dx*dx + dy*dy <= 100)
                vesa_putpixel(x+12+dx, y+12+dy, 0x0078D4);
    /* i harfi */
    vesa_fill_rect(x+11, y+6, 2, 2, COLOR_TEXT_WHITE);
    vesa_fill_rect(x+11, y+10, 2, 8, COLOR_TEXT_WHITE);
}

/* 24x24 kapat ikonu */
static inline void draw_icon_power(int x, int y) {
    for (int dy = -9; dy <= 9; dy++)
        for (int dx = -9; dx <= 9; dx++) {
            int d = dx*dx + dy*dy;
            if (d <= 81 && d >= 49)
                vesa_putpixel(x+12+dx, y+12+dy, COLOR_TEXT_RED);
        }
    vesa_fill_rect(x+11, y+3, 2, 10, COLOR_TEXT_RED);
}

/* 16x16 kapat (X) butonu */
static inline void draw_close_button(int x, int y, uint32_t bg) {
    vesa_fill_rect(x, y, 16, 16, bg);
    for (int i = 0; i < 10; i++) {
        vesa_putpixel(x+3+i, y+3+i, COLOR_TEXT_WHITE);
        vesa_putpixel(x+12-i, y+3+i, COLOR_TEXT_WHITE);
        vesa_putpixel(x+4+i, y+3+i, COLOR_TEXT_WHITE);
        vesa_putpixel(x+11-i, y+3+i, COLOR_TEXT_WHITE);
    }
}

/* Windows logo (basit 4 kare) */
static inline void draw_windows_logo(int x, int y, uint32_t color) {
    vesa_fill_rect(x, y, 7, 7, color);
    vesa_fill_rect(x+9, y, 7, 7, color);
    vesa_fill_rect(x, y+9, 7, 7, color);
    vesa_fill_rect(x+9, y+9, 7, 7, color);
}

#endif
