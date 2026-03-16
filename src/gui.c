#include "gui.h"
#include "vesa.h"
#include "mouse.h"
#include "keyboard.h"
#include "string.h"
#include "icons.h"
#include "io.h"
#include "shell.h"

static int start_open = 0;
static int window_open = 0; /* 0=yok 1=sysinfo 2=about */

static uint8_t bcd2bin(uint8_t b) { return ((b>>4)*10)+(b&0x0F); }

static void get_time(char* buf) {
    outb(0x70,0x04); uint8_t h=bcd2bin(inb(0x71));
    outb(0x70,0x02); uint8_t m=bcd2bin(inb(0x71));
    buf[0]='0'+h/10; buf[1]='0'+h%10; buf[2]=':';
    buf[3]='0'+m/10; buf[4]='0'+m%10; buf[5]='\0';
}

/* Masaustu ikonlari */
typedef struct { int x,y; const char* label; int id; } desktop_icon_t;
static desktop_icon_t icons[] = {
    {30, 40, "Bilgisayarim", 1},
    {30, 120, "Terminal", 2},
    {30, 200, "Hakkinda", 3},
};
#define ICON_COUNT 3

static void draw_desktop_icons(void) {
    for (int i = 0; i < ICON_COUNT; i++) {
        int ix=icons[i].x, iy=icons[i].y;
        if (icons[i].id == 1) draw_icon_computer(ix, iy);
        else if (icons[i].id == 2) draw_icon_terminal(ix, iy);
        else draw_icon_info(ix, iy);
        int tw = k_strlen(icons[i].label) * 8;
        vesa_draw_string_nobg(ix + 12 - tw/2, iy+28, icons[i].label, COLOR_TEXT_WHITE);
    }
}

static void draw_taskbar(void) {
    int sw = vesa_get_width();
    int sh = vesa_get_height();
    int th = 40; /* taskbar yuksekligi */
    int ty = sh - th;

    vesa_fill_rect(0, ty, sw, th, COLOR_TASKBAR);
    /* Ust cizgi */
    vesa_fill_rect(0, ty, sw, 1, COLOR_WINDOW_BORDER);

    /* Baslat butonu */
    uint32_t start_color = start_open ? COLOR_START_HOVER : COLOR_START_BTN;
    vesa_fill_rect(4, ty+4, 80, 32, start_color);
    draw_windows_logo(12, ty+10, COLOR_TEXT_WHITE);
    vesa_draw_string(32, ty+12, "Baslat", COLOR_TEXT_WHITE, start_color);

    /* Saat */
    char clock[6];
    get_time(clock);
    vesa_draw_string(sw-52, ty+12, clock, COLOR_TEXT_WHITE, COLOR_TASKBAR);

    /* Ortada MyOS */
    vesa_draw_string(sw/2-36, ty+12, "MyOS v0.3", COLOR_TEXT_GREY, COLOR_TASKBAR);
}

static void draw_start_menu(void) {
    int sh = vesa_get_height();
    int mx=0, my=sh-40-220, mw=240, mh=220;

    vesa_fill_rect(mx, my, mw, mh, COLOR_MENU_BG);
    vesa_draw_rect_outline(mx, my, mw, mh, COLOR_WINDOW_BORDER);

    /* Kullanici */
    vesa_fill_rect(mx+10, my+10, mw-20, 30, COLOR_WINDOW_TITLE);
    draw_windows_logo(mx+18, my+15, COLOR_ACCENT);
    vesa_draw_string(mx+46, my+17, "MyOS User", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);

    /* Menu ogeleri */
    const char* items[] = {"Terminal", "Sistem Bilgisi", "Hakkinda", "Yeniden Baslat", "Kapat"};
    for (int i = 0; i < 5; i++) {
        int iy = my + 50 + i * 32;
        vesa_draw_string(mx+20, iy+8, items[i], COLOR_TEXT_WHITE, COLOR_MENU_BG);
        if (i == 4)
            draw_icon_power(mx+mw-34, iy+4);
    }
}

static void show_sysinfo_window(void) {
    int wx=180, wy=80, ww=440, wh=350;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Sistem Bilgisi", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24, wy+8, COLOR_CLOSE_BTN);

    char vendor[13];
    uint32_t ebx, ecx, edx;
    __asm__ __volatile__("cpuid":"=b"(ebx),"=c"(ecx),"=d"(edx):"a"(0));
    *((uint32_t*)&vendor[0])=ebx;
    *((uint32_t*)&vendor[4])=edx;
    *((uint32_t*)&vendor[8])=ecx;
    vendor[12]='\0';

    int cy = wy+50;
    draw_icon_computer(wx+20, cy);
    vesa_draw_string(wx+60, cy+4, "CPU:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+100, cy+4, vendor, COLOR_TEXT_GREEN, COLOR_WINDOW_BG);
    cy += 40;
    vesa_draw_string(wx+60, cy, "Mimari:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+130, cy, "i686 32-bit", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    cy += 30;
    vesa_draw_string(wx+60, cy, "Video:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    char res[20];
    k_itoa(vesa_get_width(), res, 10);
    int l = k_strlen(res);
    res[l]='x'; k_itoa(vesa_get_height(), res+l+1, 10);
    vesa_draw_string(wx+130, cy, res, COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    cy += 30;
    vesa_draw_string(wx+60, cy, "Versiyon:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+130, cy, "MyOS v0.3", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    cy += 30;
    vesa_draw_string(wx+60, cy, "Klavye:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+130, cy, "PS/2", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    cy += 30;
    vesa_draw_string(wx+60, cy, "Mouse:", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+130, cy, "PS/2 Mouse", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
}

static void show_about_window(void) {
    int wx=200, wy=120, ww=400, wh=260;
    vesa_fill_rect(wx, wy, ww, wh, COLOR_WINDOW_BG);
    vesa_fill_rect(wx, wy, ww, 32, COLOR_WINDOW_TITLE);
    vesa_draw_rect_outline(wx, wy, ww, wh, COLOR_WINDOW_BORDER);
    vesa_draw_string(wx+16, wy+8, "Hakkinda", COLOR_TEXT_WHITE, COLOR_WINDOW_TITLE);
    draw_close_button(wx+ww-24, wy+8, COLOR_CLOSE_BTN);

    draw_windows_logo(wx+ww/2-8, wy+50, COLOR_ACCENT);
    vesa_draw_string(wx+ww/2-28, wy+80, "MyOS", COLOR_TEXT_WHITE, COLOR_WINDOW_BG);
    vesa_draw_string(wx+ww/2-52, wy+100, "Versiyon 0.3.0", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+40, wy+140, "Sifirdan yazilmis isletim sistemi", COLOR_TEXT_CYAN, COLOR_WINDOW_BG);
    vesa_draw_string(wx+40, wy+170, "x86 32-bit | VESA Grafik", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
    vesa_draw_string(wx+40, wy+200, "C + Assembly ile yazildi", COLOR_TEXT_GREY, COLOR_WINDOW_BG);
}

static void do_shutdown(void) {
    vesa_fill_screen(COLOR_BLACK);
    vesa_draw_string(320, 290, "Kapatiliyor...", COLOR_TEXT_WHITE, COLOR_BLACK);
    vesa_copy_buffer();
    outb(0x604, 0x2000);
    outb(0xB004, 0x2000);
    outb(0x4004, 0x3400);
    __asm__ __volatile__("cli; hlt");
}

void gui_run(void) {
    mouse_state_t ms;
    int sw = vesa_get_width();
    int sh = vesa_get_height();

    while (1) {
        /* Arka plan */
        vesa_fill_screen(COLOR_BG);

        /* Masaustu ikonlari */
        draw_desktop_icons();

        /* Pencere aciksa ciz */
        if (window_open == 1) show_sysinfo_window();
        if (window_open == 2) show_about_window();

        /* Gorev cubugu */
        draw_taskbar();

        /* Baslat menusu */
        if (start_open) draw_start_menu();

        /* Mouse */
        mouse_poll(&ms);
        mouse_draw_cursor(ms.x, ms.y);

        /* Ekrana bas */
        vesa_copy_buffer();

        /* ---- Tiklama ---- */
        if (ms.click) {
            /* Pencere kapatma */
            if (window_open == 1) {
                int wx=180,wy=80,ww=440;
                if (ms.x >= wx+ww-24 && ms.x < wx+ww-8 && ms.y >= wy+8 && ms.y < wy+24) {
                    window_open = 0; continue;
                }
            }
            if (window_open == 2) {
                int wx=200,wy=120,ww=400;
                if (ms.x >= wx+ww-24 && ms.x < wx+ww-8 && ms.y >= wy+8 && ms.y < wy+24) {
                    window_open = 0; continue;
                }
            }

            /* Baslat butonu */
            if (ms.x >= 4 && ms.x < 84 && ms.y >= sh-36 && ms.y < sh-4) {
                start_open = !start_open;
                window_open = 0;
                continue;
            }

            /* Baslat menu tiklama */
            if (start_open) {
                int mx2=0, my2=sh-40-220;
                for (int i = 0; i < 5; i++) {
                    int iy = my2 + 50 + i * 32;
                    if (ms.x >= mx2 && ms.x < mx2+240 && ms.y >= iy && ms.y < iy+32) {
                        start_open = 0;
                        if (i == 0) {
                            /* Terminal - text mode'a gec */
                            /* Basit terminal penceresi */
                            window_open = 0;
                            /* TODO: grafik terminal */
                        }
                        else if (i == 1) { window_open = 1; }
                        else if (i == 2) { window_open = 2; }
                        else if (i == 3) { outb(0x64, 0xFE); }
                        else if (i == 4) { do_shutdown(); }
                        break;
                    }
                }
                /* Menu disina tiklanirsa kapat */
                if (ms.x > 240 || ms.y < (sh-40-220)) {
                    start_open = 0;
                }
                continue;
            }

            /* Masaustu ikonlari */
            for (int i = 0; i < ICON_COUNT; i++) {
                int ix=icons[i].x, iy=icons[i].y;
                if (ms.x >= ix && ms.x < ix+60 && ms.y >= iy && ms.y < iy+50) {
                    if (icons[i].id == 1) window_open = 1;
                    else if (icons[i].id == 2) { /* terminal */ }
                    else if (icons[i].id == 3) window_open = 2;
                    start_open = 0;
                    break;
                }
            }
        }
    }
}
