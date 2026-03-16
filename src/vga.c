#include "vga.h"
#include "io.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t* vga_buffer;
static int cursor_x, cursor_y;
static uint8_t current_color;

static uint8_t make_color(uint8_t fg, uint8_t bg) {
    return fg | (bg << 4);
}

static uint16_t make_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_init(void) {
    vga_buffer = (uint16_t*)VGA_MEMORY;
    cursor_x = 0;
    cursor_y = 0;
    current_color = make_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_buffer[i] = make_entry(' ', current_color);
    cursor_x = 0;
    cursor_y = 0;
    vga_update_cursor();
}

void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = make_color(fg, bg);
}

static void vga_scroll(void) {
    if (cursor_y >= VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++)
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
            vga_buffer[i] = make_entry(' ', current_color);
        cursor_y = VGA_HEIGHT - 1;
    }
}

void vga_putchar(char c) {
    if (c == '\n') { cursor_x = 0; cursor_y++; }
    else if (c == '\r') { cursor_x = 0; }
    else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; }
    } else {
        vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_entry(c, current_color);
        if (++cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; }
    }
    vga_scroll();
    vga_update_cursor();
}

void vga_print(const char* str) {
    while (*str) vga_putchar(*str++);
}

void vga_print_colored(const char* str, uint8_t fg, uint8_t bg) {
    uint8_t old = current_color;
    current_color = make_color(fg, bg);
    vga_print(str);
    current_color = old;
}

void vga_backspace(void) {
    if (cursor_x > 0) cursor_x--;
    else if (cursor_y > 0) { cursor_y--; cursor_x = VGA_WIDTH - 1; }
    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = make_entry(' ', current_color);
    vga_update_cursor();
}

void vga_update_cursor(void) {
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 14); outb(0x3D5, (pos >> 8) & 0xFF);
    outb(0x3D4, 15); outb(0x3D5, pos & 0xFF);
}
