#include "gui.h"
#include "vga.h"
#include "mouse.h"
#include "io.h"
#include <stdint.h>

typedef struct {
    int x, y, w, h;
    const char* label;
    uint8_t fg, bg;
} button_t;

static button_t btn_about  = {  4,  4, 14, 3, "ABOUT",     VGA_WHITE, VGA_CYAN  };
static button_t btn_reboot = { 22,  4, 14, 3, "REBOOT",    VGA_WHITE, VGA_GREEN };
static button_t btn_power  = { 40,  4, 18, 3, "POWER OFF", VGA_WHITE, VGA_RED   };
static button_t btn_ok     = { 35, 16, 10, 3, "OK",        VGA_WHITE, VGA_BLUE  };

static int about_open = 0;
static int cursor_drawn = 0;
static int cursor_draw_x = 0, cursor_draw_y = 0;
static uint16_t cursor_saved = 0;

static int str_len(const char* s) {
    int n = 0; while (s[n]) n++; return n;
}

static void fill_rect(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            vga_putentry_at(' ', fg, bg, xx, yy);
}

static void draw_frame(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int xx = x; xx < x + w; xx++) {
        vga_putentry_at('-', fg, bg, xx, y);
        vga_putentry_at('-', fg, bg, xx, y + h - 1);
    }
    for (int yy = y; yy < y + h; yy++) {
        vga_putentry_at('|', fg, bg, x, yy);
        vga_putentry_at('|', fg, bg, x + w - 1, yy);
    }
    vga_putentry_at('+', fg, bg, x, y);
    vga_putentry_at('+', fg, bg, x + w - 1, y);
    vga_putentry_at('+', fg, bg, x, y + h - 1);
    vga_putentry_at('+', fg, bg, x + w - 1, y + h - 1);
}

static void draw_button(const button_t* b) {
    fill_rect(b->x, b->y, b->w, b->h, b->fg, b->bg);
    draw_frame(b->x, b->y, b->w, b->h, b->fg, b->bg);
    int len = str_len(b->label);
    int tx = b->x + (b->w - len) / 2;
    int ty = b->y + b->h / 2;
    vga_write_at(b->label, tx, ty, b->fg, b->bg);
}

static int inside_button(const button_t* b, int x, int y) {
    return (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h);
}

static void cursor_hide(void) {
    if (!cursor_drawn) return;
    vga_setentry_at(cursor_saved, cursor_draw_x, cursor_draw_y);
    cursor_drawn = 0;
}

static void cursor_show(int x, int y) {
    cursor_hide();
    cursor_saved = vga_getentry_at(x, y);
    uint8_t color = (uint8_t)(cursor_saved >> 8);
    uint8_t ofg = color & 0x0F;
    uint8_t obg = (color >> 4) & 0x0F;
    char c = (char)(cursor_saved & 0xFF);
    if (!c || c == ' ') c = ' ';
    vga_putentry_at(c, obg, ofg, x, y);
    cursor_draw_x = x; cursor_draw_y = y; cursor_drawn = 1;
}

static void set_status(const char* msg) {
    fill_rect(0, 24, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(msg, 2, 24, VGA_WHITE, VGA_DARK_GREY);
}

static void draw_about_window(void) {
    fill_rect(20, 8, 40, 11, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(20, 8, 40, 11, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(21, 9, 38, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" About MyOS ", 23, 9, VGA_WHITE, VGA_BLUE);
    vga_write_at("Mini OS denemesi", 24, 12, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("x86 32-bit / C + ASM", 24, 13, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Mouse ile secilebilir menu", 24, 14, VGA_BLACK, VGA_LIGHT_GREY);
    draw_button(&btn_ok);
}

static void draw_desktop(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();
    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Desktop ", 2, 0, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at("Kurulum Tamamlandi!", 55, 0, VGA_LIGHT_GREEN, VGA_DARK_GREY);
    draw_button(&btn_about);
    draw_button(&btn_reboot);
    draw_button(&btn_power);
    vga_write_at("Fare ile butonlara tikla.", 4, 10, VGA_WHITE, VGA_BLUE);
    set_status("Hazir.");
    if (about_open) draw_about_window();
}

static void handle_click(int x, int y) {
    if (about_open) {
        if (inside_button(&btn_ok, x, y)) { about_open = 0; draw_desktop(); }
        return;
    }
    if (inside_button(&btn_about, x, y)) { about_open = 1; draw_desktop(); return; }
    if (inside_button(&btn_reboot, x, y)) {
        outb(0x64, 0xFE);
        for (;;) __asm__ __volatile__("cli; hlt");
    }
    if (inside_button(&btn_power, x, y)) {
        set_status("Kapatiliyor...");
        outw(0x604, 0x2000);
        outw(0xB004, 0x2000);
        outw(0x4004, 0x3400);
        for (;;) __asm__ __volatile__("cli; hlt");
    }
}

void gui_run(void) {
    mouse_state_t ms;
    int last_left = 0;

    about_open = 0;
    vga_hide_cursor();

    draw_desktop();
    mouse_get_state(&ms);
    cursor_show(ms.x, ms.y);

    for (;;) {
        if (mouse_poll(&ms)) {
            cursor_hide();

            if (about_open) {
                if (inside_button(&btn_ok, ms.x, ms.y))
                    set_status("Pencereyi kapat.");
                else
                    set_status("About penceresi acik.");
            } else {
                if (inside_button(&btn_about, ms.x, ms.y))
                    set_status("Hakkinda penceresini ac.");
                else if (inside_button(&btn_reboot, ms.x, ms.y))
                    set_status("Sistemi yeniden baslat.");
                else if (inside_button(&btn_power, ms.x, ms.y))
                    set_status("Sistemi kapat.");
                else
                    set_status("Fare ile buton sec.");
            }

            if (ms.left && !last_left)
                handle_click(ms.x, ms.y);

            cursor_show(ms.x, ms.y);
            last_left = ms.left;
        }
    }
}
