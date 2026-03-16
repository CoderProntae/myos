#include "gui.h"
#include "vga.h"
#include "mouse.h"
#include "keyboard.h"
#include "io.h"
#include "string.h"

typedef struct {
    int x, y, w, h;
    const char* label;
    uint8_t fg, bg;
} button_t;

static button_t buttons[] = {
    {  4,  4, 16, 3, "ABOUT",        VGA_WHITE, VGA_CYAN   },
    { 22,  4, 16, 3, "SYSTEM INFO",  VGA_WHITE, VGA_MAGENTA},
    { 40,  4, 16, 3, "COLOR DEMO",   VGA_WHITE, VGA_BROWN  },
    {  4,  9, 16, 3, "TERMINAL",     VGA_WHITE, VGA_DARK_GREY },
    { 22,  9, 16, 3, "REBOOT",       VGA_WHITE, VGA_GREEN  },
    { 40,  9, 16, 3, "POWER OFF",    VGA_WHITE, VGA_RED    },
};
#define BTN_COUNT 6
#define BTN_ABOUT    0
#define BTN_SYSINFO  1
#define BTN_COLOR    2
#define BTN_TERMINAL 3
#define BTN_REBOOT   4
#define BTN_POWER    5

static button_t btn_ok = { 35, 18, 10, 3, "OK", VGA_WHITE, VGA_BLUE };

static int popup_open = 0;
static int popup_type = 0;

static int cursor_drawn = 0;
static int cursor_dx = 0, cursor_dy = 0;
static uint16_t cursor_saved = 0;

static int str_len(const char* s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void fill_rect(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            vga_putentry_at(' ', fg, bg, xx, yy);
}

static void draw_frame(int x, int y, int w, int h, uint8_t fg, uint8_t bg) {
    for (int xx = x; xx < x + w; xx++) {
        vga_putentry_at(196, fg, bg, xx, y);
        vga_putentry_at(196, fg, bg, xx, y + h - 1);
    }
    for (int yy = y; yy < y + h; yy++) {
        vga_putentry_at(179, fg, bg, x, yy);
        vga_putentry_at(179, fg, bg, x + w - 1, yy);
    }
    vga_putentry_at(218, fg, bg, x, y);
    vga_putentry_at(191, fg, bg, x + w - 1, y);
    vga_putentry_at(192, fg, bg, x, y + h - 1);
    vga_putentry_at(217, fg, bg, x + w - 1, y + h - 1);
}

static void draw_button(const button_t* b) {
    fill_rect(b->x, b->y, b->w, b->h, b->fg, b->bg);
    int len = str_len(b->label);
    int tx = b->x + (b->w - len) / 2;
    int ty = b->y + b->h / 2;
    vga_write_at(b->label, tx, ty, b->fg, b->bg);
}

static int inside(const button_t* b, int x, int y) {
    return (x >= b->x && x < b->x + b->w && y >= b->y && y < b->y + b->h);
}

static void cursor_hide(void) {
    if (!cursor_drawn) return;
    vga_setentry_at(cursor_saved, cursor_dx, cursor_dy);
    cursor_drawn = 0;
}

static void cursor_show(int x, int y) {
    cursor_hide();
    cursor_saved = vga_getentry_at(x, y);
    uint8_t col = (uint8_t)(cursor_saved >> 8);
    uint8_t ofg = col & 0x0F;
    uint8_t obg = (col >> 4) & 0x0F;
    char c = (char)(cursor_saved & 0xFF);
    if (!c || c == ' ') c = ' ';
    vga_putentry_at(c, obg, ofg, x, y);
    cursor_dx = x;
    cursor_dy = y;
    cursor_drawn = 1;
}

static void set_status(const char* msg) {
    fill_rect(0, 24, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(msg, 2, 24, VGA_WHITE, VGA_DARK_GREY);
}

static void draw_popup_about(void) {
    fill_rect(15, 10, 50, 11, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(15, 10, 50, 11, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(16, 11, 48, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" About MyOS ", 30, 11, VGA_WHITE, VGA_BLUE);
    vga_write_at("MyOS v0.2 - Mini isletim sistemi", 18, 13, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("x86 32-bit / C + Assembly", 18, 14, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Mouse + Klavye destegi", 18, 15, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("GUI + Terminal modlari", 18, 16, VGA_BLACK, VGA_LIGHT_GREY);
    draw_button(&btn_ok);
}

static void draw_popup_sysinfo(void) {
    fill_rect(15, 10, 50, 11, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(15, 10, 50, 11, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(16, 11, 48, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" System Info ", 30, 11, VGA_WHITE, VGA_BLUE);

    char vendor[13];
    uint32_t ebx, ecx, edx;
    __asm__ __volatile__("cpuid" : "=b"(ebx),"=c"(ecx),"=d"(edx) : "a"(0));
    *((uint32_t*)&vendor[0]) = ebx;
    *((uint32_t*)&vendor[4]) = edx;
    *((uint32_t*)&vendor[8]) = ecx;
    vendor[12] = '\0';

    vga_write_at("CPU    : ", 18, 13, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at(vendor, 27, 13, VGA_BLUE, VGA_LIGHT_GREY);
    vga_write_at("Mimari : i686 (32-bit)", 18, 14, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Video  : VGA 80x25 text", 18, 15, VGA_BLACK, VGA_LIGHT_GREY);
    vga_write_at("Giris  : PS/2 klavye + mouse", 18, 16, VGA_BLACK, VGA_LIGHT_GREY);
    draw_button(&btn_ok);
}

static void draw_popup_color(void) {
    fill_rect(15, 10, 50, 11, VGA_BLACK, VGA_LIGHT_GREY);
    draw_frame(15, 10, 50, 11, VGA_BLACK, VGA_LIGHT_GREY);
    fill_rect(16, 11, 48, 1, VGA_WHITE, VGA_BLUE);
    vga_write_at(" Color Palette ", 29, 11, VGA_WHITE, VGA_BLUE);

    for (int i = 0; i < 16; i++) {
        int px = 18 + (i % 8) * 5;
        int py = 13 + (i / 8) * 2;
        vga_putentry_at(' ', (uint8_t)i, (uint8_t)i, px, py);
        vga_putentry_at(' ', (uint8_t)i, (uint8_t)i, px + 1, py);
        vga_putentry_at(' ', (uint8_t)i, (uint8_t)i, px + 2, py);

        char num[4];
        k_itoa(i, num, 10);
        vga_write_at(num, px, py + 1, VGA_BLACK, VGA_LIGHT_GREY);
    }
    draw_button(&btn_ok);
}

static void draw_desktop(void) {
    vga_set_color(VGA_WHITE, VGA_BLUE);
    vga_clear();

    fill_rect(0, 0, 80, 1, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at(" MyOS Desktop v0.2 ", 2, 0, VGA_WHITE, VGA_DARK_GREY);
    vga_write_at("[T]=Terminal  Mouse=Click", 50, 0, VGA_YELLOW, VGA_DARK_GREY);

    for (int i = 0; i < BTN_COUNT; i++)
        draw_button(&buttons[i]);

    vga_write_at("Fare ile butonlara tiklayin.", 4, 14, VGA_LIGHT_CYAN, VGA_BLUE);
    vga_write_at("Klavyeden [T] = Terminal modu", 4, 16, VGA_LIGHT_GREY, VGA_BLUE);
    vga_write_at("[ESC] = Popup kapat", 4, 17, VGA_LIGHT_GREY, VGA_BLUE);

    set_status("Hazir. Bir buton secin.");

    if (popup_open) {
        if (popup_type == BTN_ABOUT)   draw_popup_about();
        if (popup_type == BTN_SYSINFO) draw_popup_sysinfo();
        if (popup_type == BTN_COLOR)   draw_popup_color();
    }
}

static void reboot_system(void) {
    outb(0x64, 0xFE);
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    for (;;) __asm__ __volatile__("cli; hlt");
}

static void poweroff_system(void) {
    set_status("Kapatiliyor...");
    outw(0x604,  0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
    for (;;) __asm__ __volatile__("cli; hlt");
}

static void update_hover(int x, int y) {
    if (popup_open) {
        if (inside(&btn_ok, x, y))
            set_status("Pencereyi kapat.");
        else
            set_status("Pencere acik. [OK] veya [ESC] ile kapat.");
        return;
    }
    for (int i = 0; i < BTN_COUNT; i++) {
        if (inside(&buttons[i], x, y)) {
            set_status(buttons[i].label);
            return;
        }
    }
    set_status("Hazir. Bir buton secin.");
}

static int handle_click(int x, int y) {
    if (popup_open) {
        if (inside(&btn_ok, x, y)) {
            popup_open = 0;
            draw_desktop();
        }
        return GUI_EXIT_NONE;
    }

    for (int i = 0; i < BTN_COUNT; i++) {
        if (!inside(&buttons[i], x, y)) continue;

        if (i == BTN_ABOUT || i == BTN_SYSINFO || i == BTN_COLOR) {
            popup_open = 1;
            popup_type = i;
            draw_desktop();
            return GUI_EXIT_NONE;
        }
        if (i == BTN_TERMINAL) return GUI_EXIT_TERMINAL;
        if (i == BTN_REBOOT)   reboot_system();
        if (i == BTN_POWER)    poweroff_system();
    }
    return GUI_EXIT_NONE;
}

static int check_keyboard(void) {
    uint8_t st = inb(0x64);
    if (!(st & 1)) return 0;

    uint8_t sc = inb(0x60);

    if (st & 0x20) return 0;

    if (sc & 0x80) return 0;

    if (sc == 0x14) return GUI_EXIT_TERMINAL;

    if (sc == 0x01 && popup_open) {
        popup_open = 0;
        draw_desktop();
    }

    return GUI_EXIT_NONE;
}

int gui_run(void) {
    mouse_state_t ms;
    int last_left = 0;

    popup_open = 0;
    vga_hide_cursor();
    mouse_init();
    mouse_get_state(&ms);
    draw_desktop();
    cursor_show(ms.x, ms.y);

    for (;;) {
        int kb = check_keyboard();
        if (kb == GUI_EXIT_TERMINAL) {
            cursor_hide();
            return GUI_EXIT_TERMINAL;
        }

        if (mouse_poll(&ms)) {
            cursor_hide();
            update_hover(ms.x, ms.y);

            if (ms.left && !last_left) {
                int r = handle_click(ms.x, ms.y);
                if (r == GUI_EXIT_TERMINAL) {
                    return GUI_EXIT_TERMINAL;
                }
            }
            cursor_show(ms.x, ms.y);
            last_left = ms.left;
        } else {
            last_left = ms.left;
        }
    }
}
