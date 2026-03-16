#ifndef VGA_H
#define VGA_H

#include <stdint.h>

enum vga_color {
    VGA_BLACK = 0, VGA_BLUE = 1, VGA_GREEN = 2, VGA_CYAN = 3,
    VGA_RED = 4, VGA_MAGENTA = 5, VGA_BROWN = 6, VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8, VGA_LIGHT_BLUE = 9, VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11, VGA_LIGHT_RED = 12, VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14, VGA_WHITE = 15,
};

void vga_init(void);
void vga_clear(void);
void vga_clear_color(uint8_t fg, uint8_t bg);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_putchar(char c);
void vga_putchar_at(char c, uint8_t color, int x, int y);
void vga_print(const char* str);
void vga_print_at(const char* str, uint8_t fg, uint8_t bg, int x, int y);
void vga_print_colored(const char* str, uint8_t fg, uint8_t bg);
void vga_backspace(void);
void vga_update_cursor(void);
void vga_hide_cursor(void);
void vga_fill_row(int row, uint8_t fg, uint8_t bg);
void vga_fill_rect(int x1, int y1, int x2, int y2, uint8_t fg, uint8_t bg);
void vga_draw_box(int x1, int y1, int x2, int y2, uint8_t fg, uint8_t bg);
uint16_t vga_get_entry(int x, int y);
void vga_set_entry(int x, int y, uint16_t entry);

#endif
